#include "camera/CameraValidation.h"

#include <cmath>
#include <ostream>

#include <glm/glm.hpp>

#include "camera/CameraController.h"

namespace
{
bool finitePose(const CameraPose& pose)
{
    return std::isfinite(pose.position.x) && std::isfinite(pose.position.y) &&
           std::isfinite(pose.position.z) && std::isfinite(pose.yaw) &&
           std::isfinite(pose.pitch) && std::isfinite(pose.fovDegrees);
}

bool close(float a, float b, float tolerance = 1.0e-3f)
{
    return std::abs(a - b) <= tolerance;
}

bool close(const glm::vec3& a, const glm::vec3& b, float tolerance = 1.0e-3f)
{
    return glm::distance(a, b) <= tolerance;
}
} // namespace

bool validateCameraNavigation(std::ostream& output)
{
    bool presetsValid = CameraController::presets().size() == 9;
    for (const CameraPose& preset : CameraController::presets())
        presetsValid = presetsValid && finitePose(preset) &&
                       preset.pitch >= -89.0f && preset.pitch <= 89.0f &&
                       preset.fovDegrees >= CameraController::minimumFov &&
                       preset.fovDegrees <= CameraController::maximumFov;

    const CameraPose start{{1.0f, 2.0f, 3.0f}, 170.0f, -10.0f, 45.0f,
                           "Start", "Validation"};
    const CameraPose target{{9.0f, 8.0f, 7.0f}, -170.0f, 20.0f, 60.0f,
                            "Target", "Validation"};
    const CameraPose atStart = CameraController::interpolatePose(start, target, 0.0f);
    const CameraPose atEnd = CameraController::interpolatePose(start, target, 1.0f);
    const CameraPose midpoint = CameraController::interpolatePose(start, target, 0.5f);
    CameraController transition60;
    CameraController transition30;
    transition60.selectPreset(8, false);
    transition30.selectPreset(8, false);
    for (int frame = 0; frame < 60; ++frame)
        transition60.update(1.0f / 60.0f, glm::vec3{0.0f});
    for (int frame = 0; frame < 30; ++frame)
        transition30.update(1.0f / 30.0f, glm::vec3{0.0f});
    const bool transitionValid = close(atStart.position, start.position) &&
                                 close(atStart.yaw, start.yaw) &&
                                 close(atEnd.position, target.position) &&
                                 close(atEnd.yaw, target.yaw) && finitePose(midpoint) &&
                                 std::abs(std::abs(midpoint.yaw) - 180.0f) < 0.01f &&
                                 close(transition60.camera().Position,
                                       transition30.camera().Position, 0.02f) &&
                                 transition60.mode() == CameraMode::Free &&
                                 transition30.mode() == CameraMode::Free;

    CameraController cancelController;
    cancelController.selectPreset(3, false);
    const bool transitionStarted =
        cancelController.mode() == CameraMode::PresetTransition;
    cancelController.handleMouseDelta(2.0f, 1.0f);
    CameraController demoCancelController;
    demoCancelController.toggleGuidedDemo();
    demoCancelController.move(CameraMovement::LEFT, 0.01f, CameraSpeedMode::Slow);
    const bool manualCancelValid = transitionStarted &&
                                   cancelController.mode() == CameraMode::Free &&
                                   finitePose(cancelController.currentPose()) &&
                                   demoCancelController.mode() == CameraMode::Free &&
                                   finitePose(demoCancelController.currentPose());

    CameraController orbitController;
    orbitController.togglePyramidOrbit();
    orbitController.handleMouseDelta(800.0f, 800.0f);
    orbitController.handleScroll(-1000.0f);
    const glm::vec3 pyramidTarget{0.0f, 20.0f, -42.0f};
    const glm::vec3 expectedDirection = glm::normalize(
        pyramidTarget - orbitController.camera().Position);
    const bool orbitValid = orbitController.mode() == CameraMode::OrbitPyramid &&
                            orbitController.orbitRadius() >=
                                CameraController::minimumOrbitRadius &&
                            orbitController.orbitRadius() <=
                                CameraController::maximumOrbitRadius &&
                            orbitController.camera().Pitch <= 70.0f &&
                            orbitController.camera().Pitch >= -89.0f &&
                            glm::dot(expectedDirection,
                                     orbitController.camera().Front) > 0.999f &&
                            finitePose(orbitController.currentPose());

    const glm::vec3 transportTarget{0.0f, 2.0f, 25.0f};
    const glm::vec3 desired = CameraController::followDesiredPosition(transportTarget);
    const float desiredDistance = glm::distance(desired, transportTarget);
    CameraController follow60;
    CameraController follow30;
    follow60.toggleTransportFollow(transportTarget);
    follow30.toggleTransportFollow(transportTarget);
    for (int frame = 0; frame < 60; ++frame)
        follow60.update(1.0f / 60.0f, transportTarget);
    for (int frame = 0; frame < 30; ++frame)
        follow30.update(1.0f / 30.0f, transportTarget);
    const bool followValid = desiredDistance >= 15.0f && desiredDistance <= 30.0f &&
                             finitePose(follow60.currentPose()) &&
                             close(follow60.camera().Position,
                                   follow30.camera().Position, 0.02f);

    CameraController movement60;
    CameraController movement30;
    for (int frame = 0; frame < 60; ++frame)
        movement60.move(CameraMovement::FORWARD, 1.0f / 60.0f,
                        CameraSpeedMode::Normal);
    for (int frame = 0; frame < 30; ++frame)
        movement30.move(CameraMovement::FORWARD, 1.0f / 30.0f,
                        CameraSpeedMode::Normal);
    CameraController slowMovement;
    CameraController fastMovement;
    CameraController verticalMovement;
    const glm::vec3 movementStart = slowMovement.camera().Position;
    slowMovement.move(CameraMovement::FORWARD, 1.0f, CameraSpeedMode::Slow);
    fastMovement.move(CameraMovement::FORWARD, 1.0f, CameraSpeedMode::Fast);
    verticalMovement.move(CameraMovement::DOWN, 1.0f, CameraSpeedMode::Normal);
    CameraController mouseController;
    mouseController.handleMouseDelta(0.0f, 2000.0f);
    const bool upperPitchClamped = close(mouseController.camera().Pitch, 89.0f);
    mouseController.handleMouseDelta(0.0f, -4000.0f);
    const bool lowerPitchClamped = close(mouseController.camera().Pitch, -89.0f);
    CameraController resetController;
    resetController.handleScroll(5.0f);
    resetController.move(CameraMovement::LEFT, 0.2f, CameraSpeedMode::Fast);
    resetController.reset();
    const bool movementValid = close(movement60.camera().Position,
                                     movement30.camera().Position, 0.01f) &&
                               close(glm::distance(movementStart,
                                                   slowMovement.camera().Position), 7.0f) &&
                               close(glm::distance(movementStart,
                                                   fastMovement.camera().Position), 56.0f) &&
                               close(movementStart.y - verticalMovement.camera().Position.y,
                                     24.0f) &&
                               upperPitchClamped && lowerPitchClamped &&
                               close(resetController.camera().Position,
                                     CameraController::presets()[0].position) &&
                               resetController.speedMode() == CameraSpeedMode::Normal &&
                               close(CameraController::speedFor(CameraSpeedMode::Slow), 7.0f) &&
                               close(CameraController::speedFor(CameraSpeedMode::Normal), 24.0f) &&
                               close(CameraController::speedFor(CameraSpeedMode::Fast), 56.0f);

    CameraController demoController;
    demoController.toggleGuidedDemo();
    demoController.update(2.0f, transportTarget);
    const bool demoValid = demoController.mode() == CameraMode::GuidedDemo &&
                           finitePose(demoController.currentPose()) &&
                           CameraController::demoShots().size() == 7;

    constexpr float aspect = 16.0f / 9.0f;
    const bool projectionValid = CameraController::nearPlane > 0.0f &&
                                 CameraController::farPlane > CameraController::nearPlane &&
                                 aspect > 0.0f &&
                                 CameraController::minimumFov >= 1.0f &&
                                 CameraController::maximumFov < 90.0f;

    const bool valid = presetsValid && transitionValid && manualCancelValid &&
                       orbitValid && followValid && movementValid && demoValid &&
                       projectionValid;
    output << "Phase 6 camera/navigation validation\n"
           << "  nine finite distinct-purpose presets: "
           << (presetsValid ? "PASS" : "FAIL") << "\n"
           << "  transition endpoints and wrapped yaw: "
           << (transitionValid ? "PASS" : "FAIL") << "\n"
           << "  manual input cancels automation: "
           << (manualCancelValid ? "PASS" : "FAIL") << "\n"
           << "  pyramid orbit radius/elevation/look-at: "
           << (orbitValid ? "PASS" : "FAIL") << "\n"
           << "  transport follow and frame independence: "
           << (followValid ? "PASS" : "FAIL") << "\n"
           << "  free movement speeds and frame independence: "
           << (movementValid ? "PASS" : "FAIL") << "\n"
           << "  seven-shot guided demo: " << (demoValid ? "PASS" : "FAIL") << "\n"
           << "  projection limits: " << (projectionValid ? "PASS" : "FAIL") << "\n"
           << (valid ? "Camera/navigation checks passed.\n"
                     : "Camera/navigation checks failed.\n");
    return valid;
}
