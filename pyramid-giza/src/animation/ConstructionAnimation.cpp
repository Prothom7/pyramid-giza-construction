#include "animation/ConstructionAnimation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "scene/SceneTypes.h"
#include "scene/MonumentalSite.h"

namespace
{
constexpr float idleDuration = 1.5f;
constexpr float approachDuration = 3.0f;
constexpr float readyDuration = 1.5f;
constexpr float groundPullDuration = 6.0f;
constexpr float rampApproachDuration = 2.0f;
constexpr float rampPullDuration = 6.0f;
constexpr float arrivalDuration = 1.5f;
constexpr float leverDuration = 3.0f;
constexpr float placementDuration = 2.0f;
constexpr float completeDuration = 2.0f;

constexpr glm::vec3 sledgeStart{10.0f, 0.0f, 40.0f};
constexpr glm::vec3 groundTurn{0.0f, 0.0f, 47.0f};
constexpr float groundEndProgress = 0.35f;
constexpr float rampEntryProgress = 0.45f;

constexpr std::array<glm::vec3, 7> staticPositions{{
    {7.5f, 0.0f, 35.0f}, {12.0f, 0.0f, 35.0f}, {-123.0f, -7.42f, -16.0f},
    {-108.0f, -7.42f, -3.0f}, {5.0f, 0.0f, 2.0f}, {23.0f, 0.0f, 12.0f},
    {14.0f, 0.0f, 32.0f}
}};
constexpr std::array<float, 7> staticHeadings{{0.0f, 0.0f, -80.0f, 155.0f, -35.0f, 90.0f, 170.0f}};

float saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smooth(float value)
{
    const float t = saturate(value);
    return t * t * (3.0f - 2.0f * t);
}

glm::vec3 lerp(const glm::vec3& from, const glm::vec3& to, float amount)
{
    return glm::mix(from, to, smooth(amount));
}

float headingFor(const glm::vec3& direction)
{
    return glm::degrees(std::atan2(-direction.x, -direction.z));
}

glm::vec3 horizontalDirection(const glm::vec3& from, const glm::vec3& to)
{
    glm::vec3 direction = to - from;
    direction.y = 0.0f;
    const float length = glm::length(direction);
    return length > 1.0e-6f ? direction / length : glm::vec3{0.0f, 0.0f, -1.0f};
}

glm::mat4 workerRoot(const glm::vec3& position, float heading)
{
    return makeTransform(position, {0.0f, heading, 0.0f}, {1.0f, 1.0f, 1.0f});
}

void blendField(glm::vec3& result, const glm::vec3& from, const glm::vec3& to, float amount)
{
    result = glm::mix(from, to, saturate(amount));
}

float stateProgress(float stateTime, ConstructionState state)
{
    return smooth(stateTime / ConstructionAnimationController::stateDuration(state));
}
} // namespace

void ConstructionAnimationController::update(float deltaTime)
{
    if (paused_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    float remaining = deltaTime * speed_;
    while (remaining > 0.0f)
    {
        const float duration = stateDuration(state_);
        const float available = std::max(0.0f, duration - stateTime_);
        const float step = std::min(remaining, available);
        stateTime_ += step;
        elapsedTime_ += step;
        remaining -= step;

        if (stateTime_ + 1.0e-6f >= duration)
            finishState();
        else
            break;

        if (paused_)
            break;
    }
}

void ConstructionAnimationController::reset()
{
    state_ = ConstructionState::Idle;
    stateTime_ = 0.0f;
    elapsedTime_ = 0.0f;
    paused_ = false;
}

void ConstructionAnimationController::advanceState()
{
    if (state_ == ConstructionState::Complete && !looping_)
        return;
    state_ = nextState(state_);
    stateTime_ = 0.0f;
    if (state_ == ConstructionState::Idle)
        elapsedTime_ = 0.0f;
}

void ConstructionAnimationController::setSpeed(float speed)
{
    if (!std::isfinite(speed))
        return;
    speed_ = std::clamp(speed, 0.25f, 4.0f);
}

ConstructionState ConstructionAnimationController::nextState(ConstructionState state)
{
    switch (state)
    {
    case ConstructionState::Idle: return ConstructionState::WorkersApproach;
    case ConstructionState::WorkersApproach: return ConstructionState::PullReady;
    case ConstructionState::PullReady: return ConstructionState::PullGround;
    case ConstructionState::PullGround: return ConstructionState::ApproachRamp;
    case ConstructionState::ApproachRamp: return ConstructionState::RampPull;
    case ConstructionState::RampPull: return ConstructionState::Arrival;
    case ConstructionState::Arrival: return ConstructionState::LeverPreparation;
    case ConstructionState::LeverPreparation: return ConstructionState::PlacementReady;
    case ConstructionState::PlacementReady: return ConstructionState::Complete;
    case ConstructionState::Complete: return ConstructionState::Idle;
    default: throw std::out_of_range("Unknown construction animation state");
    }
}

void ConstructionAnimationController::finishState()
{
    if (state_ == ConstructionState::Complete)
    {
        if (looping_)
        {
            state_ = ConstructionState::Idle;
            stateTime_ = 0.0f;
            elapsedTime_ = 0.0f;
        }
        else
        {
            stateTime_ = stateDuration(state_);
            paused_ = true;
        }
        return;
    }
    state_ = nextState(state_);
    stateTime_ = 0.0f;
}

float ConstructionAnimationController::stateDuration(ConstructionState state)
{
    switch (state)
    {
    case ConstructionState::Idle: return idleDuration;
    case ConstructionState::WorkersApproach: return approachDuration;
    case ConstructionState::PullReady: return readyDuration;
    case ConstructionState::PullGround: return groundPullDuration;
    case ConstructionState::ApproachRamp: return rampApproachDuration;
    case ConstructionState::RampPull: return rampPullDuration;
    case ConstructionState::Arrival: return arrivalDuration;
    case ConstructionState::LeverPreparation: return leverDuration;
    case ConstructionState::PlacementReady: return placementDuration;
    case ConstructionState::Complete: return completeDuration;
    default: throw std::out_of_range("Unknown construction animation state");
    }
}

float ConstructionAnimationController::sequenceDuration()
{
    return idleDuration + approachDuration + readyDuration + groundPullDuration +
           rampApproachDuration + rampPullDuration + arrivalDuration + leverDuration +
           placementDuration + completeDuration;
}

const char* ConstructionAnimationController::stateName(ConstructionState state)
{
    switch (state)
    {
    case ConstructionState::Idle: return "Idle";
    case ConstructionState::WorkersApproach: return "WorkersApproach";
    case ConstructionState::PullReady: return "PullReady";
    case ConstructionState::PullGround: return "PullGround";
    case ConstructionState::ApproachRamp: return "ApproachRamp";
    case ConstructionState::RampPull: return "RampPull";
    case ConstructionState::Arrival: return "Arrival";
    case ConstructionState::LeverPreparation: return "LeverPreparation";
    case ConstructionState::PlacementReady: return "PlacementReady";
    case ConstructionState::Complete: return "Complete";
    default: throw std::out_of_range("Unknown construction animation state");
    }
}

float ConstructionAnimationController::rampSurfaceHeight(float z)
{
    return MonumentalSite::mainRampSurfaceHeight(z);
}

TransportFrame ConstructionAnimationController::transportAt(float progress)
{
    const RampDescriptor& ramp = MonumentalSite::mainRamp();
    const glm::vec3 rampEntry{ramp.base.x, rampSurfaceHeight(ramp.base.z), ramp.base.z};
    const glm::vec3 rampTop{ramp.top.x, rampSurfaceHeight(ramp.top.z), ramp.top.z};
    const float rampDegrees = MonumentalSite::mainRampPitchDegrees();
    const float p = saturate(progress);
    glm::vec3 position;
    glm::vec3 direction;
    float pitch = 0.0f;
    float heading = 0.0f;

    const glm::vec3 entryWithHeight = rampEntry;
    const glm::vec3 topWithHeight = rampTop;
    const glm::vec3 groundDirection = horizontalDirection(sledgeStart, groundTurn);

    if (p <= groundEndProgress)
    {
        const float local = p / groundEndProgress;
        position = lerp(sledgeStart, groundTurn, local);
        direction = groundDirection;
        heading = headingFor(direction);
    }
    else if (p <= rampEntryProgress)
    {
        const float local = (p - groundEndProgress) / (rampEntryProgress - groundEndProgress);
        position = lerp(groundTurn, entryWithHeight, local);
        direction = horizontalDirection(groundTurn, entryWithHeight);
        heading = glm::mix(headingFor(groundDirection), 0.0f, smooth(local));
        pitch = glm::mix(0.0f, rampDegrees, smooth(local));
    }
    else
    {
        const float local = (p - rampEntryProgress) / (1.0f - rampEntryProgress);
        position = lerp(entryWithHeight, topWithHeight, local);
        direction = horizontalDirection(entryWithHeight, topWithHeight);
        heading = headingFor(direction);
        pitch = rampDegrees;
    }

    const glm::vec3 side{-direction.z, 0.0f, direction.x};
    constexpr float leadDistance = 3.7f;
    constexpr float pairSpacing = 0.48f;
    glm::vec3 left = position + direction * leadDistance - side * pairSpacing;
    glm::vec3 right = position + direction * leadDistance + side * pairSpacing;
    if (p > groundEndProgress)
    {
        left.y = rampSurfaceHeight(left.z);
        right.y = rampSurfaceHeight(right.z);
    }
    else
    {
        left.y = 0.0f;
        right.y = 0.0f;
    }
    return {position, left, right, heading, pitch};
}

WorkerJointAngles ConstructionAnimationController::blendPoses(
    const WorkerJointAngles& from, const WorkerJointAngles& to, float amount)
{
    WorkerJointAngles result;
    blendField(result.pelvis, from.pelvis, to.pelvis, amount);
    blendField(result.torso, from.torso, to.torso, amount);
    blendField(result.neck, from.neck, to.neck, amount);
    blendField(result.head, from.head, to.head, amount);
    blendField(result.leftShoulder, from.leftShoulder, to.leftShoulder, amount);
    blendField(result.leftElbow, from.leftElbow, to.leftElbow, amount);
    blendField(result.leftWrist, from.leftWrist, to.leftWrist, amount);
    blendField(result.rightShoulder, from.rightShoulder, to.rightShoulder, amount);
    blendField(result.rightElbow, from.rightElbow, to.rightElbow, amount);
    blendField(result.rightWrist, from.rightWrist, to.rightWrist, amount);
    blendField(result.leftHip, from.leftHip, to.leftHip, amount);
    blendField(result.leftKnee, from.leftKnee, to.leftKnee, amount);
    blendField(result.leftAnkle, from.leftAnkle, to.leftAnkle, amount);
    blendField(result.rightHip, from.rightHip, to.rightHip, amount);
    blendField(result.rightKnee, from.rightKnee, to.rightKnee, amount);
    blendField(result.rightAnkle, from.rightAnkle, to.rightAnkle, amount);
    return Worker::clampJointAngles(result);
}

WorkerJointAngles ConstructionAnimationController::walkingPose(
    const WorkerJointAngles& base, float time, float strength, bool pulling)
{
    WorkerJointAngles angles = base;
    const float wave = std::sin(time * 5.0f);
    const float opposite = -wave;
    const float gait = saturate(strength);
    angles.leftHip.x += 18.0f * wave * gait;
    angles.rightHip.x += 18.0f * opposite * gait;
    angles.leftKnee.x += 24.0f * std::max(0.0f, opposite) * gait;
    angles.rightKnee.x += 24.0f * std::max(0.0f, wave) * gait;
    const float armAmplitude = pulling ? 6.0f : 16.0f;
    angles.leftShoulder.x += armAmplitude * opposite * gait;
    angles.rightShoulder.x += armAmplitude * wave * gait;
    angles.torso.y += 2.5f * wave * gait;
    return Worker::clampJointAngles(angles);
}

glm::mat4 ConstructionAnimationController::cylinderBetween(
    const glm::vec3& start, const glm::vec3& end, float diameter)
{
    const glm::vec3 displacement = end - start;
    const float length = glm::length(displacement);
    if (!std::isfinite(length) || length < 1.0e-5f || diameter <= 0.0f)
        throw std::invalid_argument("Cylinder endpoints and diameter must define a positive segment");

    const glm::vec3 direction = displacement / length;
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const float cosine = std::clamp(glm::dot(up, direction), -1.0f, 1.0f);
    glm::mat4 model = glm::translate(glm::mat4{1.0f}, (start + end) * 0.5f);
    if (cosine < 1.0f - 1.0e-6f)
    {
        if (cosine <= -1.0f + 1.0e-6f)
            model = glm::rotate(model, glm::pi<float>(), {1.0f, 0.0f, 0.0f});
        else
            model = glm::rotate(model, std::acos(cosine), glm::normalize(glm::cross(up, direction)));
    }
    return glm::scale(model, {diameter, length, diameter});
}

glm::mat4 ConstructionAnimationController::toolAttachmentRoot(const glm::mat4& handJointWorld)
{
    return glm::translate(handJointWorld, {0.0f, -0.04f, 0.0f});
}

glm::mat4 ConstructionAnimationController::leverRoot()
{
    return makeTransform({23.0f, 0.0f, 12.0f}, {0.0f, 90.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
}

glm::mat4 ConstructionAnimationController::leverPivotFrame(float angleDegrees)
{
    glm::mat4 pivot = glm::translate(leverRoot(), {0.0f, 0.55f, 0.0f});
    return glm::rotate(pivot, glm::radians(68.0f + angleDegrees), {0.0f, 0.0f, 1.0f});
}

glm::mat4 ConstructionAnimationController::leverBeamModel(float angleDegrees)
{
    glm::mat4 model = glm::translate(leverPivotFrame(angleDegrees), {0.0f, 0.45f, 0.0f});
    return glm::scale(model, {0.16f, 3.10f, 0.16f});
}

glm::mat4 ConstructionAnimationController::leverStoneModel(float liftOffset)
{
    glm::mat4 model = glm::translate(leverRoot(), {-1.25f, 0.35f + liftOffset, 0.0f});
    return glm::scale(model, {0.90f, 0.70f, 0.90f});
}

ConstructionAnimationSnapshot ConstructionAnimationController::snapshot() const
{
    ConstructionAnimationSnapshot result;
    result.state = state_;
    result.stateProgress = stateProgress(stateTime_, state_);

    for (std::size_t index = 0; index < result.workers.size(); ++index)
    {
        result.workers[index].root = workerRoot(staticPositions[index], staticHeadings[index]);
        result.workers[index].joints = Worker::poseAngles(WorkerPose::Standing);
    }

    float transportProgress = 0.0f;
    if (state_ == ConstructionState::PullGround)
        transportProgress = groundEndProgress * result.stateProgress;
    else if (state_ == ConstructionState::ApproachRamp)
        transportProgress = glm::mix(groundEndProgress, rampEntryProgress, result.stateProgress);
    else if (state_ == ConstructionState::RampPull)
        transportProgress = glm::mix(rampEntryProgress, 1.0f, result.stateProgress);
    else if (state_ >= ConstructionState::Arrival)
        transportProgress = 1.0f;
    result.transportProgress = transportProgress;

    const TransportFrame transport = transportAt(transportProgress);
    result.loadedSledgeRoot = makeTransform(
        transport.sledgePosition,
        {transport.rampPitchDegrees, transport.headingDegrees, 0.0f},
        {1.0f, 1.0f, 1.0f});

    const auto leftIndex = static_cast<std::size_t>(WorkerRole::PullerLeft);
    const auto rightIndex = static_cast<std::size_t>(WorkerRole::PullerRight);
    const WorkerJointAngles standing = Worker::poseAngles(WorkerPose::Standing);
    const WorkerJointAngles pulling = Worker::poseAngles(WorkerPose::PullingReady);

    if (state_ == ConstructionState::WorkersApproach)
    {
        const glm::vec3 left = lerp(staticPositions[leftIndex], transport.leftWorkerPosition,
                                    result.stateProgress);
        const glm::vec3 right = lerp(staticPositions[rightIndex], transport.rightWorkerPosition,
                                     result.stateProgress);
        result.workers[leftIndex].root = workerRoot(left, transport.headingDegrees);
        result.workers[rightIndex].root = workerRoot(right, transport.headingDegrees);
        result.workers[leftIndex].joints = walkingPose(standing, elapsedTime_, 1.0f, false);
        result.workers[rightIndex].joints = walkingPose(standing, elapsedTime_ + 0.63f, 1.0f, false);
    }
    else if (state_ == ConstructionState::PullReady)
    {
        result.workers[leftIndex].root = workerRoot(transport.leftWorkerPosition, transport.headingDegrees);
        result.workers[rightIndex].root = workerRoot(transport.rightWorkerPosition, transport.headingDegrees);
        result.workers[leftIndex].joints = blendPoses(standing, pulling, result.stateProgress);
        result.workers[rightIndex].joints = blendPoses(standing, pulling, result.stateProgress);
        result.ropeVisible = true;
    }
    else if (state_ >= ConstructionState::PullGround)
    {
        result.workers[leftIndex].root = workerRoot(transport.leftWorkerPosition, transport.headingDegrees);
        result.workers[rightIndex].root = workerRoot(transport.rightWorkerPosition, transport.headingDegrees);
        const bool moving = state_ == ConstructionState::PullGround ||
                            state_ == ConstructionState::ApproachRamp ||
                            state_ == ConstructionState::RampPull;
        WorkerJointAngles leftJoints = moving ? walkingPose(pulling, elapsedTime_, 1.0f, true) : pulling;
        WorkerJointAngles rightJoints = moving ? walkingPose(pulling, elapsedTime_ + 0.63f, 1.0f, true) : pulling;
        if (state_ == ConstructionState::Arrival)
        {
            leftJoints = blendPoses(pulling, standing, result.stateProgress);
            rightJoints = blendPoses(pulling, standing, result.stateProgress);
        }
        else if (state_ > ConstructionState::Arrival)
        {
            leftJoints = standing;
            rightJoints = standing;
        }
        result.workers[leftIndex].joints = leftJoints;
        result.workers[rightIndex].joints = rightJoints;
        result.ropeVisible = state_ <= ConstructionState::Arrival;
    }

    const auto quarryIndex = static_cast<std::size_t>(WorkerRole::QuarryMallet);
    WorkerJointAngles quarry = Worker::poseAngles(WorkerPose::Standing);
    const float strike = 0.5f + 0.5f * std::sin(elapsedTime_ * 3.2f);
    quarry.rightShoulder = {glm::mix(25.0f, 105.0f, strike), 0.0f, 24.0f};
    quarry.rightElbow.x = glm::mix(35.0f, 90.0f, strike);
    quarry.torso.x = glm::mix(-4.0f, 12.0f, strike);
    result.workers[quarryIndex].joints = Worker::clampJointAngles(quarry);

    const auto carrierIndex = static_cast<std::size_t>(WorkerRole::QuarryCarrier);
    result.workers[carrierIndex].joints = walkingPose(
        Worker::poseAngles(WorkerPose::CarryingReady), elapsedTime_ + 0.9f, 0.25f, false);

    const auto rampIndex = static_cast<std::size_t>(WorkerRole::RampGuide);
    if (state_ == ConstructionState::RampPull || state_ == ConstructionState::Arrival)
    {
        glm::vec3 guidePosition = transport.sledgePosition + glm::vec3{2.0f, 0.0f, -0.4f};
        guidePosition.y = rampSurfaceHeight(guidePosition.z);
        result.workers[rampIndex].root = workerRoot(guidePosition, transport.headingDegrees);
        result.workers[rampIndex].joints = walkingPose(
            Worker::poseAngles(WorkerPose::CarryingReady), elapsedTime_ + 0.3f,
            state_ == ConstructionState::RampPull ? 0.7f : 0.0f, false);
    }
    else
        result.workers[rampIndex].joints = Worker::poseAngles(WorkerPose::CarryingReady);

    const auto leverIndex = static_cast<std::size_t>(WorkerRole::LeverOperator);
    const WorkerJointAngles leverPose = Worker::poseAngles(WorkerPose::LeverReady);
    if (state_ == ConstructionState::LeverPreparation)
    {
        result.workers[leverIndex].joints = blendPoses(standing, leverPose, result.stateProgress);
        result.leverAngleDegrees = -12.0f * result.stateProgress;
    }
    else if (state_ >= ConstructionState::PlacementReady)
    {
        result.workers[leverIndex].joints = leverPose;
        result.leverAngleDegrees = -12.0f;
        result.liftedStoneOffset = state_ == ConstructionState::PlacementReady
                                       ? 0.24f * result.stateProgress
                                       : 0.24f;
    }
    else
        result.workers[leverIndex].joints = standing;

    const auto stockpileIndex = static_cast<std::size_t>(WorkerRole::StockpileWorker);
    result.workers[stockpileIndex].joints = blendPoses(
        standing, Worker::poseAngles(WorkerPose::CarryingReady),
        0.35f + 0.15f * std::sin(elapsedTime_ * 1.4f));

    for (AnimatedWorkerState& worker : result.workers)
        worker.joints = Worker::clampJointAngles(worker.joints);
    return result;
}
