#include "presentation/ShowcaseController.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <vector>

#include "animation/ConstructionAnimation.h"
#include "scene/PyramidLayout.h"

namespace
{
float smoothStep(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool finite3(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool finitePose(const CameraPose& pose)
{
    return finite3(pose.position) && std::isfinite(pose.yaw) &&
           std::isfinite(pose.pitch) && std::isfinite(pose.fovDegrees) &&
           pose.fovDegrees >= CameraController::minimumFov &&
           pose.fovDegrees <= CameraController::maximumFov;
}

bool safeCameraPosition(const glm::vec3& position)
{
    if (!finite3(position) || position.y < 0.5f ||
        position.x < -225.0f || position.x > 185.0f ||
        position.z < -225.0f || position.z > 150.0f)
        return false;

    // Conservative completed-pyramid core. The real sloped monument occupies
    // less volume, so this intentionally keeps the camera farther away.
    const bool insidePyramidCore =
        position.x > -45.0f && position.x < 45.0f &&
        position.z > -87.0f && position.z < 3.0f &&
        position.y < 61.0f;
    // Only reject low camera samples inside the solid quarry-wall region.
    const bool insideQuarryWall =
        position.x > -150.0f && position.x < -92.0f &&
        position.z > -48.0f && position.z < 20.0f &&
        position.y < 7.5f;
    return !insidePyramidCore && !insideQuarryWall;
}

bool near(float left, float right, float tolerance = 1.0e-4f)
{
    return std::abs(left - right) <= tolerance;
}
} // namespace

const std::array<ShowcaseShot, ShowcaseController::shotCount>&
ShowcaseController::shots()
{
    static const std::array<ShowcaseShot, shotCount> values{{
        {"OpeningOverview", "Monumental site scale, ramp, and desert context",
         0.0f, 5.0f, {125.0f, 75.0f, 105.0f}, {105.0f, 62.0f, 90.0f},
         {0.0f, 18.0f, -35.0f}, {0.0f, 20.0f, -42.0f}, 52.0f, 48.0f,
         8.0f, 8.35f,
         ShowcaseConstructionAction::SetCheckpoint, 0.25f, 1.0f, false, false},
        {"QuarryOverview", "Terraces, extraction bays, workers, and haul route",
         5.0f, 5.0f, {-70.0f, 40.0f, 60.0f}, {-90.0f, 28.0f, 42.0f},
         {-118.0f, -3.0f, -10.0f}, {-124.0f, -5.0f, -15.0f}, 52.0f, 49.0f,
         8.35f, 8.70f,
         ShowcaseConstructionAction::SetCheckpoint, 0.35f, 1.0f, false, true},
        {"RockExtraction", "Limestone channel cutting, chisel work, and bed separation",
         10.0f, 5.0f, {-96.0f, 16.0f, 24.0f}, {-112.0f, 11.0f, -5.0f},
         {-122.0f, -6.0f, -14.0f}, {-123.0f, -6.8f, -16.0f}, 48.0f, 44.0f,
         8.70f, 9.05f,
         ShowcaseConstructionAction::SetCheckpoint, 0.40f, 1.0f, false, false},
        {"RockShaping", "Stone masons dressing rough stone into square blocks",
         15.0f, 5.0f, {-80.0f, 25.0f, 50.0f}, {-40.0f, 18.0f, 58.0f},
         {-55.0f, 1.0f, 29.0f}, {-25.0f, 1.0f, 37.0f}, 50.0f, 47.0f,
         9.05f, 9.40f,
         ShowcaseConstructionAction::SetCheckpoint, 0.50f, 1.0f, false, true},
        {"SledgeLoading", "Loaded sledge, preparation area, and transport start",
         20.0f, 5.0f, {-35.0f, 14.0f, 62.0f}, {12.0f, 11.0f, 62.0f},
         {-10.0f, 1.0f, 41.0f}, {10.0f, 1.0f, 40.0f}, 48.0f, 45.0f,
         9.40f, 9.75f,
         ShowcaseConstructionAction::SetCheckpoint, 0.75f, 1.0f, true, false},
        {"WorkersHauling", "Pullers, ropes, sledge, and restrained runner dust",
         25.0f, 6.0f, {25.0f, 12.0f, 66.0f}, {14.0f, 11.0f, 48.0f},
         {7.0f, 1.2f, 40.0f}, {2.0f, 2.0f, 35.0f}, 46.0f, 44.0f,
         9.75f, 10.20f,
         ShowcaseConstructionAction::KeepCheckpoint, 0.75f, 1.0f, false, false},
        {"RampAscent", "Heavy gait, incline, rope, dust, and scaffold",
         31.0f, 7.0f, {28.0f, 18.0f, 38.0f}, {24.0f, 22.0f, 12.0f},
         {0.0f, 5.0f, 17.0f}, {0.0f, 12.0f, -2.0f}, 47.0f, 45.0f,
         10.20f, 10.75f,
         ShowcaseConstructionAction::KeepCheckpoint, 0.75f, 1.0f, false, false},
        {"PulleyLift", "Quarry pulley lifting rig, carriage, and tension",
         38.0f, 6.0f, {-78.0f, 22.0f, 42.0f}, {-88.0f, 16.0f, 32.0f},
         {-91.0f, 2.0f, 22.0f}, {-91.0f, 4.0f, 22.0f}, 48.0f, 45.0f,
         10.75f, 11.20f,
         ShowcaseConstructionAction::KeepCheckpoint, 0.75f, 1.0f, false, true},
        {"ConstructionFrontier", "Upper platform, lifting rigs, lever, and frontier",
         44.0f, 6.0f, {52.0f, 30.0f, 10.0f}, {58.0f, 38.0f, -12.0f},
         {0.0f, 18.0f, -25.0f}, {0.0f, 28.0f, -42.0f}, 46.0f, 44.0f,
         11.20f, 11.65f,
         ShowcaseConstructionAction::KeepCheckpoint, 0.75f, 1.0f, false, false},
        {"BlockPlacement", "Block placement, lever alignment, and course seating",
         50.0f, 6.0f, {48.0f, 26.0f, 18.0f}, {36.0f, 22.0f, 28.0f},
         {0.0f, 16.0f, 12.0f}, {0.0f, 18.0f, 0.0f}, 46.0f, 44.0f,
         11.65f, 12.10f,
         ShowcaseConstructionAction::KeepCheckpoint, 0.75f, 1.0f, false, false},
        {"PyramidOverview", "Stable wide view of construction from lower to summit",
         56.0f, 18.0f, {100.0f, 60.0f, 85.0f}, {86.0f, 58.0f, 72.0f},
         {0.0f, 18.0f, -42.0f}, {0.0f, 26.0f, -42.0f}, 50.0f, 47.0f,
         12.10f, 14.80f,
         ShowcaseConstructionAction::StartTimelapse, 0.18f, 3.075f, false, true},
        {"Nile", "Nile, floodplain, boats, landing, and vegetation",
         74.0f, 7.0f, {120.0f, 40.0f, -190.0f}, {145.0f, 36.0f, -165.0f},
         {130.0f, 0.0f, -105.0f}, {105.0f, 2.0f, -95.0f}, 52.0f, 50.0f,
         14.80f, 15.60f,
         ShowcaseConstructionAction::CompletePyramid, 1.0f, 1.0f, false, true},
        {"Sphinx", "Procedural anatomical Sphinx monument and context",
         81.0f, 6.0f, {132.0f, 24.0f, -132.0f}, {145.0f, 30.0f, -115.0f},
         {112.0f, 5.0f, -102.0f}, {112.0f, 5.0f, -102.0f}, 50.0f, 50.0f,
         15.60f, 16.15f,
         ShowcaseConstructionAction::KeepCheckpoint, 1.0f, 1.0f, false, false},
        {"SandWind", "Desert wind transport, sand dunes, and drift accumulation",
         87.0f, 6.0f, {75.0f, 30.0f, 48.0f}, {62.0f, 24.0f, 58.0f},
         {0.0f, 10.0f, 15.0f}, {-15.0f, 6.0f, 10.0f}, 48.0f, 46.0f,
         16.15f, 16.60f,
         ShowcaseConstructionAction::KeepCheckpoint, 1.0f, 1.0f, false, true},
        {"FinalSunsetWide", "Final complete monument, site context, and golden hour",
         93.0f, 8.0f, {120.0f, 58.0f, 90.0f}, {102.0f, 54.0f, 74.0f},
         {0.0f, 25.0f, -42.0f}, {0.0f, 27.0f, -42.0f}, 50.0f, 47.0f,
         16.60f, 17.0f,
         ShowcaseConstructionAction::KeepCheckpoint, 1.0f, 1.0f, false, true}
    }};
    return values;
}

float ShowcaseController::totalDuration()
{
    const ShowcaseShot& finalShot = shots().back();
    return finalShot.startTime + finalShot.duration;
}

std::size_t ShowcaseController::findShot(float time)
{
    const float clamped = std::clamp(time, 0.0f, totalDuration());
    for (std::size_t index = 0; index + 1 < shots().size(); ++index)
        if (clamped < shots()[index + 1].startTime)
            return index;
    return shots().size() - 1;
}

ShowcaseFrame ShowcaseController::evaluate(float time)
{
    ShowcaseFrame result;
    result.showcaseTime = std::clamp(time, 0.0f, totalDuration());
    result.shotIndex = findShot(result.showcaseTime);
    const ShowcaseShot& shot = shots()[result.shotIndex];
    const float local = result.showcaseTime - shot.startTime;
    result.shotProgress = std::clamp(local / shot.duration, 0.0f, 1.0f);
    const float eased = smoothStep(result.shotProgress);
    const glm::vec3 position = glm::mix(shot.startPosition, shot.endPosition, eased);
    result.cameraTarget = glm::mix(shot.startTarget, shot.endTarget, eased);
    const float fov = glm::mix(shot.startFov, shot.endFov, eased);
    result.camera = CameraController::lookAtPose(position, result.cameraTarget, fov);
    result.camera.name = shot.id;
    result.camera.purpose = shot.purpose;
    result.constructionProgress = shot.constructionValue;
    result.constructionPlaying =
        shot.constructionAction == ShowcaseConstructionAction::StartTimelapse;
    if (result.constructionPlaying)
        result.constructionProgress =
            glm::mix(shot.constructionValue, 1.0f, result.shotProgress);
    else if (shot.constructionAction ==
             ShowcaseConstructionAction::CompletePyramid)
        result.constructionProgress = 1.0f;
    result.constructionSpeed =
        result.constructionPlaying ? shot.constructionSpeed : 1.0f;

    float heroStartTime = 0.0f;
    for (const ShowcaseShot& timelineShot : shots())
        if (timelineShot.resetHero &&
            timelineShot.startTime <= result.showcaseTime)
            heroStartTime = timelineShot.startTime;
    result.heroTime = std::clamp(
        result.showcaseTime - heroStartTime, 0.0f,
        ConstructionAnimationController::sequenceDuration());
    result.heroPlaying =
        result.heroTime + 1.0e-5f <
        ConstructionAnimationController::sequenceDuration();
    result.sunTime =
        glm::mix(shot.startSunTime, shot.endSunTime, eased);
    result.complete = result.showcaseTime >= totalDuration() - 1.0e-5f;
    return result;
}

void ShowcaseController::start(float time)
{
    seek(time);
    mode_ = time_ >= totalDuration() ? ShowcaseMode::Complete
                                     : ShowcaseMode::Playing;
    shotChanged_ = true;
    completedThisUpdate_ = mode_ == ShowcaseMode::Complete;
}

void ShowcaseController::cancel()
{
    mode_ = ShowcaseMode::Manual;
    shotChanged_ = false;
    completedThisUpdate_ = false;
}

void ShowcaseController::togglePaused()
{
    if (mode_ == ShowcaseMode::Playing)
        mode_ = ShowcaseMode::Paused;
    else if (mode_ == ShowcaseMode::Paused)
        mode_ = ShowcaseMode::Playing;
}

void ShowcaseController::update(float deltaTime)
{
    completedThisUpdate_ = false;
    if (mode_ != ShowcaseMode::Playing || !std::isfinite(deltaTime) ||
        deltaTime <= 0.0f)
        return;
    const std::size_t previousShot = shotIndex_;
    time_ = std::min(totalDuration(), time_ + deltaTime * speed_);
    shotIndex_ = findShot(time_);
    shotChanged_ = shotChanged_ || shotIndex_ != previousShot;
    if (time_ >= totalDuration())
    {
        mode_ = ShowcaseMode::Complete;
        completedThisUpdate_ = true;
    }
}

void ShowcaseController::seek(float time)
{
    if (!std::isfinite(time))
        return;
    time_ = std::clamp(time, 0.0f, totalDuration());
    shotIndex_ = findShot(time_);
    shotChanged_ = true;
    completedThisUpdate_ = false;
}

void ShowcaseController::setSpeed(float speed)
{
    if (std::isfinite(speed))
        speed_ = std::clamp(speed, minimumSpeed, maximumSpeed);
}

bool ShowcaseController::consumeShotChanged()
{
    const bool value = shotChanged_;
    shotChanged_ = false;
    return value;
}

bool ShowcaseController::consumeCompleted()
{
    const bool value = completedThisUpdate_;
    completedThisUpdate_ = false;
    return value;
}

ShowcaseFrame ShowcaseController::frame() const
{
    return evaluate(time_);
}

const char* ShowcaseController::modeName(ShowcaseMode mode)
{
    switch (mode)
    {
    case ShowcaseMode::Playing: return "Playing";
    case ShowcaseMode::Paused: return "Paused";
    case ShowcaseMode::Complete: return "Complete";
    case ShowcaseMode::Manual:
    default: return "Manual";
    }
}

const char* ShowcaseController::constructionActionName(
    ShowcaseConstructionAction action)
{
    switch (action)
    {
    case ShowcaseConstructionAction::SetCheckpoint: return "SetCheckpoint";
    case ShowcaseConstructionAction::StartTimelapse: return "StartTimelapse";
    case ShowcaseConstructionAction::CompletePyramid: return "CompletePyramid";
    case ShowcaseConstructionAction::KeepCheckpoint:
    default: return "KeepCheckpoint";
    }
}

bool validatePhase12ShowcaseTimeline(std::ostream& output)
{
    bool table = !ShowcaseController::shots().empty();
    float expectedStart = 0.0f;
    for (const ShowcaseShot& shot : ShowcaseController::shots())
    {
        table = table && shot.startTime >= 0.0f && shot.duration > 0.0f &&
                near(shot.startTime, expectedStart) &&
                finite3(shot.startPosition) && finite3(shot.endPosition) &&
                finite3(shot.startTarget) && finite3(shot.endTarget) &&
                std::isfinite(shot.startFov) && std::isfinite(shot.endFov) &&
                shot.startFov >= CameraController::minimumFov &&
                shot.startFov <= CameraController::maximumFov &&
                shot.endFov >= CameraController::minimumFov &&
                shot.endFov <= CameraController::maximumFov &&
                std::isfinite(shot.startSunTime) &&
                std::isfinite(shot.endSunTime) &&
                shot.startSunTime >= 6.0f && shot.endSunTime <= 18.0f &&
                std::isfinite(shot.constructionValue) &&
                shot.constructionValue >= 0.0f &&
                shot.constructionValue <= 1.0f &&
                std::isfinite(shot.constructionSpeed) &&
                shot.constructionSpeed > 0.0f;
        expectedStart = shot.startTime + shot.duration;
    }
    const bool duration = near(expectedStart, ShowcaseController::totalDuration()) &&
                          near(ShowcaseController::totalDuration(), 101.0f);
    ShowcaseController first;
    ShowcaseController second;
    first.start();
    second.start();
    for (int step = 0; step < 60; ++step) first.update(1.0f / 60.0f);
    for (int step = 0; step < 30; ++step) second.update(1.0f / 30.0f);
    const bool frameRateIndependent =
        near(first.time(), second.time(), 1.0e-3f) &&
        first.shotIndex() == second.shotIndex();
    const bool valid = table && duration && frameRateIndependent;
    output << "Phase 12 showcase timeline validation\n"
           << "  15 finite contiguous positive-duration shots: "
           << (table ? "PASS" : "FAIL") << '\n'
           << "  final shot ends at 101 seconds: "
           << (duration ? "PASS" : "FAIL") << '\n'
           << "  presentation clock frame-rate independence: "
           << (frameRateIndependent ? "PASS" : "FAIL") << '\n'
           << (valid ? "Showcase timeline checks passed.\n"
                     : "Showcase timeline checks failed.\n");
    return valid;
}

bool validatePhase12ShowcaseCamera(std::ostream& output)
{
    bool finite = true;
    bool safe = true;
    for (const ShowcaseShot& shot : ShowcaseController::shots())
        for (float amount : {0.0f, 0.5f, 1.0f})
        {
            const ShowcaseFrame frame = ShowcaseController::evaluate(
                shot.startTime + shot.duration * amount);
            finite = finite && finitePose(frame.camera) &&
                     finite3(frame.cameraTarget);
            const bool sampleSafe = safeCameraPosition(frame.camera.position);
            safe = safe && sampleSafe;
            if (!sampleSafe)
                output << "  unsafe camera sample: " << shot.id << " at "
                       << amount << " = (" << frame.camera.position.x << ", "
                       << frame.camera.position.y << ", "
                       << frame.camera.position.z << ")\n";
        }
    const ShowcaseFrame finalFrame =
        ShowcaseController::evaluate(ShowcaseController::totalDuration());
    const bool finalPyramidFramed =
        glm::distance(finalFrame.camera.position, finalFrame.cameraTarget) > 60.0f &&
        finalFrame.camera.fovDegrees >= 40.0f;
    const bool valid = finite && safe && finalPyramidFramed;
    output << "Phase 12 showcase camera validation\n"
           << "  start/middle/end camera samples finite: "
           << (finite ? "PASS" : "FAIL") << '\n'
           << "  sampled paths clear ground, pyramid core, and quarry walls: "
           << (safe ? "PASS" : "FAIL") << '\n'
           << "  completed monument fits final reveal: "
           << (finalPyramidFramed ? "PASS" : "FAIL") << '\n'
           << (valid ? "Showcase camera checks passed.\n"
                     : "Showcase camera checks failed.\n");
    return valid;
}

bool validatePhase12ShowcaseState(std::ostream& output)
{
    bool deterministic = true;
    for (float time : {0.0f, 12.5f, 31.0f, 58.0f, 76.0f, 90.0f, 101.0f})
    {
        const ShowcaseFrame first = ShowcaseController::evaluate(time);
        const ShowcaseFrame second = ShowcaseController::evaluate(time);
        deterministic = deterministic && first.shotIndex == second.shotIndex &&
                        first.camera.position == second.camera.position &&
                        first.cameraTarget == second.cameraTarget &&
                        near(first.constructionProgress,
                             second.constructionProgress) &&
                        near(first.heroTime, second.heroTime) &&
                        near(first.sunTime, second.sunTime);
    }
    ShowcaseController controller;
    controller.start(58.0f);
    const ShowcaseFrame seekFrame = controller.frame();
    const ShowcaseFrame directFrame = ShowcaseController::evaluate(58.0f);
    const bool seek = seekFrame.shotIndex == directFrame.shotIndex &&
                      seekFrame.camera.position == directFrame.camera.position &&
                      near(seekFrame.constructionProgress,
                           directFrame.constructionProgress) &&
                      near(seekFrame.heroTime, directFrame.heroTime);
    const CameraPose beforeCancel = controller.frame().camera;
    controller.cancel();
    const bool cancel = controller.mode() == ShowcaseMode::Manual &&
                        finitePose(beforeCancel);
    controller.start(ShowcaseController::totalDuration());
    const ShowcaseFrame completed = controller.frame();
    const bool completion = controller.complete() && completed.complete &&
                            near(completed.constructionProgress, 1.0f) &&
                            finitePose(completed.camera);

    PyramidLayoutConfig config;
    config.completedLevels = config.baseBlocksPerSide;
    config.partialFromLevel = config.baseBlocksPerSide;
    const std::vector<PyramidBlockPlacement> blocks =
        PyramidLayout::generateComplete(config);
    const bool finalPyramid = config.baseBlocksPerSide == 28u &&
                              blocks.size() == 7714u;
    const bool valid = deterministic && seek && cancel && completion && finalPyramid;
    output << "Phase 12 showcase state validation\n"
           << "  deterministic camera/construction/hero/sun state: "
           << (deterministic ? "PASS" : "FAIL") << '\n'
           << "  direct seek equals timestamp evaluation: "
           << (seek ? "PASS" : "FAIL") << '\n'
           << "  manual cancellation returns finite camera control: "
           << (cancel ? "PASS" : "FAIL") << '\n'
           << "  final state is complete and finite: "
           << (completion ? "PASS" : "FAIL") << '\n'
           << "  final pyramid remains 7714 blocks / 28 levels: "
           << (finalPyramid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Showcase state checks passed.\n"
                     : "Showcase state checks failed.\n");
    return valid;
}
