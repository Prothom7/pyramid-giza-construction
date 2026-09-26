#include "camera/CameraController.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/constants.hpp>

namespace
{
float smoothStep(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float shortestAngleDelta(float from, float to)
{
    return std::remainder(to - from, 360.0f);
}

bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace

CameraController::CameraController()
    : camera_(presets()[0].position, {0.0f, 1.0f, 0.0f},
              presets()[0].yaw, presets()[0].pitch),
      fovDegrees_(presets()[0].fovDegrees)
{
    camera_.MovementSpeed = normalSpeed;
}

const std::array<CameraPose, 9>& CameraController::presets()
{
    static const std::array<CameraPose, 9> values{{
        {{110.0f, 65.0f, 100.0f}, -131.0f, -17.0f, 48.0f,
         "Monumental Overview", "Pyramid scale with workers and ramp"},
        {{0.0f, 10.0f, 34.0f}, -90.0f, -7.0f, 50.0f,
         "Pyramid Base", "Worker-scale pyramid and main-ramp view"},
        {{-72.0f, 38.0f, 55.0f}, -129.0f, -24.0f, 50.0f,
         "Quarry Overview", "Terraces ramp extraction and repositories"},
        {{-98.0f, 12.0f, 22.0f}, -126.0f, -21.0f, 48.0f,
         "Extraction Close-up", "Four extraction stages and quarry workers"},
        {{-5.0f, 22.0f, 75.0f}, -135.0f, -18.0f, 50.0f,
         "Stone Logistics", "Repositories loading station and haul road"},
        {{25.0f, 18.0f, 18.0f}, -140.0f, -16.0f, 48.0f,
         "Ramp and Scaffolding", "Hero ramp upper platforms and scaffold"},
        {{-170.0f, 105.0f, 110.0f}, -43.0f, -27.0f, 52.0f,
         "Industrial Site", "Complete pyramid quarry and logistics landscape"},
        {{120.0f, 42.0f, -190.0f}, 131.0f, -10.0f, 52.0f,
         "Nile and Floodplain", "Water vegetation and plateau relationship"},
        {{132.0f, 24.0f, -132.0f}, 145.0f, -9.0f, 52.0f,
         "Sphinx Context", "Secondary landmark with broader Giza context"}
    }};
    return values;
}

const std::array<DemoShot, 7>& CameraController::demoShots()
{
    static const std::array<DemoShot, 7> values{{
        {presets()[0], 1.0f, 3.0f},
        {presets()[3], 1.0f, 3.0f},
        {presets()[4], 1.0f, 3.0f},
        {{{22.0f, 10.0f, 58.0f}, -132.0f, -14.0f, 48.0f,
          "Hero Transport", "Pullers rope loaded sledge and route"}, 1.0f, 4.0f},
        {presets()[5], 1.0f, 3.0f},
        {presets()[7], 1.0f, 3.0f},
        {presets()[8], 1.0f, 3.0f}
    }};
    return values;
}

CameraPose CameraController::currentPose() const
{
    return {camera_.Position, camera_.Yaw, camera_.Pitch, fovDegrees_,
            "Current", "Current camera state"};
}

void CameraController::setPose(const CameraPose& pose)
{
    transition_ = {};
    mode_ = CameraMode::Free;
    applyPose(pose);
    constrainToWorld();
}

CameraPose CameraController::interpolatePose(const CameraPose& start,
                                             const CameraPose& target,
                                             float progress)
{
    if (progress <= 0.0f)
        return start;
    if (progress >= 1.0f)
        return target;
    const float amount = smoothStep(progress);
    CameraPose result = target;
    result.position = glm::mix(start.position, target.position, amount);
    result.yaw = start.yaw + shortestAngleDelta(start.yaw, target.yaw) * amount;
    result.pitch = glm::mix(start.pitch, target.pitch, amount);
    result.fovDegrees = glm::mix(start.fovDegrees, target.fovDegrees, amount);
    return result;
}

CameraPose CameraController::lookAtPose(const glm::vec3& position,
                                        const glm::vec3& target,
                                        float fovDegrees)
{
    const glm::vec3 difference = target - position;
    if (!finiteVector(position) || !finiteVector(target) ||
        glm::length(difference) <= 1.0e-5f)
        throw std::invalid_argument("Camera look-at points must be finite and distinct");
    const glm::vec3 direction = glm::normalize(difference);
    const float yaw = glm::degrees(std::atan2(direction.z, direction.x));
    const float pitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
    return {position, yaw, pitch, fovDegrees, "LookAt", "Target-derived orientation"};
}

CameraPose CameraController::orbitPose(const glm::vec3& target, float radius,
                                       float azimuthDegrees, float elevationDegrees,
                                       float fovDegrees)
{
    const float safeRadius = std::clamp(radius, minimumOrbitRadius, maximumOrbitRadius);
    const float safeElevation = std::clamp(elevationDegrees, 8.0f, 70.0f);
    const float azimuth = glm::radians(azimuthDegrees);
    const float elevation = glm::radians(safeElevation);
    const glm::vec3 offset{
        safeRadius * std::cos(elevation) * std::cos(azimuth),
        safeRadius * std::sin(elevation),
        safeRadius * std::cos(elevation) * std::sin(azimuth)};
    CameraPose result = lookAtPose(target + offset, target, fovDegrees);
    result.name = "Pyramid Orbit";
    result.purpose = "Inspection around pyramid center";
    return result;
}

glm::vec3 CameraController::followDesiredPosition(const glm::vec3& target)
{
    return target + glm::vec3{11.0f, 7.0f, 15.0f};
}

float CameraController::speedFor(CameraSpeedMode mode)
{
    switch (mode)
    {
    case CameraSpeedMode::Slow: return slowSpeed;
    case CameraSpeedMode::Fast: return fastSpeed;
    case CameraSpeedMode::Normal:
    default: return normalSpeed;
    }
}

const char* CameraController::modeName(CameraMode mode)
{
    switch (mode)
    {
    case CameraMode::PresetTransition: return "PresetTransition";
    case CameraMode::OrbitPyramid: return "OrbitPyramid";
    case CameraMode::FollowTransport: return "FollowTransport";
    case CameraMode::GuidedDemo: return "GuidedDemo";
    case CameraMode::Free:
    default: return "Free";
    }
}

const char* CameraController::speedModeName(CameraSpeedMode mode)
{
    switch (mode)
    {
    case CameraSpeedMode::Slow: return "Slow";
    case CameraSpeedMode::Fast: return "Fast";
    case CameraSpeedMode::Normal:
    default: return "Normal";
    }
}

void CameraController::applyPose(const CameraPose& pose)
{
    if (!finiteVector(pose.position) || !std::isfinite(pose.yaw) ||
        !std::isfinite(pose.pitch) || !std::isfinite(pose.fovDegrees))
        throw std::invalid_argument("Camera pose must contain finite values");
    fovDegrees_ = std::clamp(pose.fovDegrees, minimumFov, maximumFov);
    camera_.SetPose(pose.position, pose.yaw, std::clamp(pose.pitch, -89.0f, 89.0f));
}

void CameraController::selectPreset(std::size_t index, bool instant)
{
    if (index >= presets().size())
        throw std::out_of_range("Unknown camera preset");
    if (instant)
    {
        transition_ = {};
        mode_ = CameraMode::Free;
        applyPose(presets()[index]);
        return;
    }
    transition_.active = true;
    transition_.elapsed = 0.0f;
    transition_.duration = transitionDuration;
    transition_.start = currentPose();
    transition_.target = presets()[index];
    mode_ = CameraMode::PresetTransition;
}

void CameraController::reset()
{
    speedMode_ = CameraSpeedMode::Normal;
    camera_.MovementSpeed = normalSpeed;
    transition_ = {};
    demoShotIndex_ = 0;
    demoShotElapsed_ = 0.0f;
    mode_ = CameraMode::Free;
    applyPose(presets()[0]);
}

void CameraController::update(float deltaTime, const glm::vec3& transportTarget)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;
    switch (mode_)
    {
    case CameraMode::PresetTransition: updateTransition(deltaTime); break;
    case CameraMode::OrbitPyramid: updateOrbitPose(); break;
    case CameraMode::FollowTransport: updateFollow(deltaTime, transportTarget); break;
    case CameraMode::GuidedDemo: updateGuidedDemo(deltaTime); break;
    case CameraMode::Free: break;
    }
    constrainToWorld();
}

void CameraController::move(CameraMovement direction, float deltaTime,
                            CameraSpeedMode speedMode)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;
    cancelForManualInput();
    speedMode_ = speedMode;
    camera_.MovementSpeed = speedFor(speedMode_);
    camera_.ProcessKeyboard(direction, deltaTime);
    constrainToWorld();
}

void CameraController::handleMouseDelta(float xOffset, float yOffset)
{
    if (!std::isfinite(xOffset) || !std::isfinite(yOffset))
        return;
    if (mode_ == CameraMode::OrbitPyramid)
    {
        orbitAzimuth_ += xOffset * 0.18f;
        orbitElevation_ = std::clamp(orbitElevation_ + yOffset * 0.14f, 8.0f, 70.0f);
        updateOrbitPose();
        return;
    }
    cancelForManualInput();
    camera_.ProcessMouseMovement(xOffset, yOffset);
}

void CameraController::handleScroll(float yOffset)
{
    if (!std::isfinite(yOffset))
        return;
    if (mode_ == CameraMode::OrbitPyramid)
    {
        orbitRadius_ = std::clamp(orbitRadius_ - yOffset * 5.0f,
                                  minimumOrbitRadius, maximumOrbitRadius);
        updateOrbitPose();
        return;
    }
    cancelForManualInput();
    fovDegrees_ = std::clamp(fovDegrees_ - yOffset * 2.0f, minimumFov, maximumFov);
}

void CameraController::cancelForManualInput()
{
    transition_.active = false;
    if (mode_ != CameraMode::Free)
        mode_ = CameraMode::Free;
}

void CameraController::constrainToWorld()
{
    camera_.Position.x = std::clamp(camera_.Position.x, -230.0f, 190.0f);
    camera_.Position.y = std::clamp(camera_.Position.y, -6.8f, 200.0f);
    camera_.Position.z = std::clamp(camera_.Position.z, -230.0f, 155.0f);
}

void CameraController::togglePyramidOrbit()
{
    if (mode_ == CameraMode::OrbitPyramid)
    {
        mode_ = CameraMode::Free;
        return;
    }
    const glm::vec3 offset = camera_.Position - orbitTarget_;
    const float length = glm::length(offset);
    orbitRadius_ = std::clamp(length, minimumOrbitRadius, maximumOrbitRadius);
    if (length > 1.0e-5f)
    {
        orbitAzimuth_ = glm::degrees(std::atan2(offset.z, offset.x));
        orbitElevation_ = glm::degrees(std::asin(
            std::clamp(offset.y / length, -1.0f, 1.0f)));
    }
    orbitElevation_ = std::clamp(orbitElevation_, 8.0f, 70.0f);
    transition_.active = false;
    mode_ = CameraMode::OrbitPyramid;
    updateOrbitPose();
}

void CameraController::toggleTransportFollow(const glm::vec3& transportTarget)
{
    if (mode_ == CameraMode::FollowTransport)
    {
        mode_ = CameraMode::Free;
        return;
    }
    if (!finiteVector(transportTarget))
        return;
    transition_.active = false;
    mode_ = CameraMode::FollowTransport;
    const CameraPose facing = lookAtPose(camera_.Position,
                                         transportTarget + glm::vec3{0.0f, 1.0f, -1.5f},
                                         fovDegrees_);
    applyPose(facing);
}

void CameraController::toggleGuidedDemo()
{
    if (mode_ == CameraMode::GuidedDemo)
    {
        mode_ = CameraMode::Free;
        return;
    }
    transition_.active = false;
    demoShotIndex_ = 0;
    demoShotElapsed_ = 0.0f;
    demoShotStart_ = currentPose();
    mode_ = CameraMode::GuidedDemo;
}

void CameraController::updateTransition(float deltaTime)
{
    transition_.elapsed = std::min(transition_.elapsed + deltaTime,
                                   transition_.duration);
    if (transition_.elapsed + 1.0e-6f >= transition_.duration)
        transition_.elapsed = transition_.duration;
    const float progress = transition_.duration > 0.0f
                               ? transition_.elapsed / transition_.duration
                               : 1.0f;
    applyPose(interpolatePose(transition_.start, transition_.target, progress));
    if (progress >= 1.0f)
    {
        transition_.active = false;
        mode_ = CameraMode::Free;
    }
}

void CameraController::updateOrbitPose()
{
    applyPose(orbitPose(orbitTarget_, orbitRadius_, orbitAzimuth_,
                        orbitElevation_, fovDegrees_));
}

void CameraController::updateFollow(float deltaTime, const glm::vec3& target)
{
    if (!finiteVector(target))
        return;
    const glm::vec3 desired = followDesiredPosition(target);
    const float amount = 1.0f - std::exp(-4.0f * deltaTime);
    const glm::vec3 position = glm::mix(camera_.Position, desired, amount);
    applyPose(lookAtPose(position, target + glm::vec3{0.0f, 1.0f, -2.0f},
                         fovDegrees_));
}

void CameraController::updateGuidedDemo(float deltaTime)
{
    float remaining = deltaTime;
    while (remaining > 0.0f && mode_ == CameraMode::GuidedDemo)
    {
        const DemoShot& shot = demoShots()[demoShotIndex_];
        const float total = shot.transitionDuration + shot.holdDuration;
        const float available = std::max(0.0f, total - demoShotElapsed_);
        const float step = std::min(remaining, available);
        demoShotElapsed_ += step;
        remaining -= step;

        if (demoShotElapsed_ < shot.transitionDuration)
            applyPose(interpolatePose(demoShotStart_, shot.pose,
                                      demoShotElapsed_ / shot.transitionDuration));
        else
            applyPose(shot.pose);

        if (demoShotElapsed_ + 1.0e-6f >= total)
        {
            demoShotIndex_ = (demoShotIndex_ + 1) % demoShots().size();
            demoShotElapsed_ = 0.0f;
            demoShotStart_ = currentPose();
        }
    }
}
