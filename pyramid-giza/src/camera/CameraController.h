#pragma once

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

#include "Camera.h"

enum class CameraMode
{
    Free,
    PresetTransition,
    OrbitPyramid,
    FollowTransport,
    GuidedDemo
};

enum class CameraSpeedMode
{
    Slow,
    Normal,
    Fast
};

struct CameraPose
{
    glm::vec3 position{0.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fovDegrees = 50.0f;
    const char* name = "Unnamed";
    const char* purpose = "";
};

struct CameraTransition
{
    bool active = false;
    float elapsed = 0.0f;
    float duration = 1.0f;
    CameraPose start;
    CameraPose target;
};

struct DemoShot
{
    CameraPose pose;
    float transitionDuration = 1.0f;
    float holdDuration = 3.0f;
};

class CameraController
{
public:
    static constexpr float slowSpeed = 7.0f;
    static constexpr float normalSpeed = 24.0f;
    static constexpr float fastSpeed = 56.0f;
    static constexpr float nearPlane = 0.7f;
    static constexpr float farPlane = 700.0f;
    static constexpr float minimumFov = 30.0f;
    static constexpr float maximumFov = 75.0f;
    static constexpr float transitionDuration = 1.0f;
    static constexpr float minimumOrbitRadius = 55.0f;
    static constexpr float maximumOrbitRadius = 180.0f;

    CameraController();

    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }
    CameraMode mode() const { return mode_; }
    CameraSpeedMode speedMode() const { return speedMode_; }
    float fovDegrees() const { return fovDegrees_; }
    float orbitRadius() const { return orbitRadius_; }
    std::size_t activeDemoShot() const { return demoShotIndex_; }
    const CameraTransition& transition() const { return transition_; }

    CameraPose currentPose() const;
    void setPose(const CameraPose& pose);
    void selectPreset(std::size_t index, bool instant);
    void reset();
    void update(float deltaTime, const glm::vec3& transportTarget);
    void move(CameraMovement direction, float deltaTime, CameraSpeedMode speedMode);
    void handleMouseDelta(float xOffset, float yOffset);
    void handleScroll(float yOffset);
    void cancelForManualInput();
    void constrainToWorld();

    void togglePyramidOrbit();
    void toggleTransportFollow(const glm::vec3& transportTarget);
    void toggleGuidedDemo();

    static const std::array<CameraPose, 9>& presets();
    static const std::array<DemoShot, 7>& demoShots();
    static CameraPose interpolatePose(const CameraPose& start,
                                      const CameraPose& target, float progress);
    static CameraPose lookAtPose(const glm::vec3& position, const glm::vec3& target,
                                 float fovDegrees = 50.0f);
    static CameraPose orbitPose(const glm::vec3& target, float radius,
                                float azimuthDegrees, float elevationDegrees,
                                float fovDegrees = 50.0f);
    static glm::vec3 followDesiredPosition(const glm::vec3& target);
    static float speedFor(CameraSpeedMode mode);
    static const char* modeName(CameraMode mode);
    static const char* speedModeName(CameraSpeedMode mode);

private:
    void applyPose(const CameraPose& pose);
    void updateTransition(float deltaTime);
    void updateOrbitPose();
    void updateFollow(float deltaTime, const glm::vec3& target);
    void updateGuidedDemo(float deltaTime);

    Camera camera_;
    CameraMode mode_ = CameraMode::Free;
    CameraSpeedMode speedMode_ = CameraSpeedMode::Normal;
    float fovDegrees_ = 50.0f;
    CameraTransition transition_;
    glm::vec3 orbitTarget_{0.0f, 20.0f, -42.0f};
    float orbitRadius_ = 90.0f;
    float orbitAzimuth_ = 45.0f;
    float orbitElevation_ = 24.0f;
    std::size_t demoShotIndex_ = 0;
    float demoShotElapsed_ = 0.0f;
    CameraPose demoShotStart_;
};
