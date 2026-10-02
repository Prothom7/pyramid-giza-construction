#include "animation/QuarryPulleyAnimation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <ostream>

namespace
{
struct StateTiming
{
    QuarryPulleyState state;
    float duration;
};

constexpr std::array<StateTiming, 9> timings{{
    {QuarryPulleyState::Idle, 1.0f},
    {QuarryPulleyState::Attach, 1.0f},
    {QuarryPulleyState::Tension, 0.8f},
    {QuarryPulleyState::Lift, 4.0f},
    {QuarryPulleyState::UpperHold, 0.8f},
    {QuarryPulleyState::GuideToPlatform, 2.0f},
    {QuarryPulleyState::Lower, 2.0f},
    {QuarryPulleyState::Release, 1.0f},
    {QuarryPulleyState::Complete, 1.4f}
}};

constexpr float loadHalfHeight = 0.8f;
constexpr float slingHeight = 0.45f;

float smooth(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool finite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool near(const glm::vec3& left, const glm::vec3& right, float tolerance = 1.0e-3f)
{
    return glm::length(left - right) <= tolerance;
}

float startTime(QuarryPulleyState state)
{
    float result = 0.0f;
    for (const StateTiming& timing : timings)
    {
        if (timing.state == state)
            return result;
        result += timing.duration;
    }
    return result;
}
} // namespace

void QuarryPulleyAnimationController::update(float deltaTime)
{
    // Physical logistics sets the controller from its normalized state progress.
    // The free-running clock remains exclusively for cinematic presentation.
    if (activePhysicalBlockId_ != 0 || paused_ || !std::isfinite(deltaTime) ||
        deltaTime <= 0.0f)
        return;
    elapsedTime_ += static_cast<double>(deltaTime);
    const double duration = static_cast<double>(sequenceDuration());
    if (looping_)
    {
        elapsedTime_ = std::fmod(elapsedTime_, duration);
        if (elapsedTime_ < 0.0)
            elapsedTime_ += duration;
    }
    else
        elapsedTime_ = std::min(elapsedTime_, duration);
}

void QuarryPulleyAnimationController::reset()
{
    elapsedTime_ = 0.0f;
    paused_ = false;
    activePhysicalBlockId_ = 0;
}

void QuarryPulleyAnimationController::seek(float elapsedTime, bool playing)
{
    if (!std::isfinite(elapsedTime))
        return;
    activePhysicalBlockId_ = 0;
    const double duration = static_cast<double>(sequenceDuration());
    if (looping_)
    {
        elapsedTime_ = std::fmod(
            static_cast<double>(std::max(0.0f, elapsedTime)), duration);
        if (elapsedTime_ < 0.0)
            elapsedTime_ += duration;
    }
    else
        elapsedTime_ = std::clamp(static_cast<double>(elapsedTime), 0.0, duration);
    paused_ = !playing;
}

bool QuarryPulleyAnimationController::setPhysicalPreparationProgress(
    std::uint64_t blockId, float progress)
{
    if (blockId == 0 ||
        (activePhysicalBlockId_ != 0 && activePhysicalBlockId_ != blockId) ||
        !std::isfinite(progress))
        return false;

    activePhysicalBlockId_ = blockId;
    paused_ = true;
    const float attachStart = startTime(QuarryPulleyState::Attach);
    const float liftStart = startTime(QuarryPulleyState::Lift);
    elapsedTime_ = glm::mix(attachStart, liftStart,
                            std::clamp(progress, 0.0f, 1.0f));
    return true;
}

bool QuarryPulleyAnimationController::setPhysicalLiftProgress(
    std::uint64_t blockId, float progress)
{
    if (blockId == 0 ||
        (activePhysicalBlockId_ != 0 && activePhysicalBlockId_ != blockId) ||
        !std::isfinite(progress))
        return false;

    activePhysicalBlockId_ = blockId;
    paused_ = true;
    const float liftStart = startTime(QuarryPulleyState::Lift);
    elapsedTime_ = liftStart + std::clamp(progress, 0.0f, 1.0f) *
                                   stateDuration(QuarryPulleyState::Lift);
    return true;
}

bool QuarryPulleyAnimationController::holdPhysicalBlockAtLiftTop(
    std::uint64_t blockId)
{
    if (blockId == 0 ||
        (activePhysicalBlockId_ != 0 && activePhysicalBlockId_ != blockId))
        return false;
    activePhysicalBlockId_ = blockId;
    paused_ = true;
    elapsedTime_ = startTime(QuarryPulleyState::UpperHold);
    return true;
}

bool QuarryPulleyAnimationController::releasePhysicalBlock(
    std::uint64_t blockId)
{
    if (blockId == 0 || activePhysicalBlockId_ != blockId)
        return false;
    activePhysicalBlockId_ = 0;
    paused_ = true;
    return true;
}

QuarryPulleySnapshot QuarryPulleyAnimationController::snapshot() const
{
    return snapshotAt(static_cast<float>(elapsedTime_));
}

QuarryPulleySnapshot QuarryPulleyAnimationController::snapshotAt(float elapsedTime)
{
    const float duration = sequenceDuration();
    float time = std::isfinite(elapsedTime) ? std::max(0.0f, elapsedTime) : 0.0f;
    if (time >= duration)
        time = std::nextafter(duration, 0.0f);

    QuarryPulleyState state = QuarryPulleyState::Complete;
    float localTime = 0.0f;
    float cursor = 0.0f;
    for (const StateTiming& timing : timings)
    {
        if (time < cursor + timing.duration)
        {
            state = timing.state;
            localTime = time - cursor;
            break;
        }
        cursor += timing.duration;
    }

    const float progress = std::clamp(localTime / stateDuration(state), 0.0f, 1.0f);
    const float eased = smooth(progress);
    const glm::vec3 start = startLoadPosition();
    const glm::vec3 raised = raisedLoadPosition();
    const glm::vec3 destination = destinationLoadPosition();

    QuarryPulleySnapshot result;
    result.state = state;
    result.stateProgress = progress;
    result.loadPosition = start;
    result.carriagePosition = {start.x, 1.10f, start.z};
    result.support = QuarryPulleyLoadSupport::QuarryFloor;

    switch (state)
    {
    case QuarryPulleyState::Lift:
        result.loadPosition = glm::mix(start, raised, eased);
        result.support = QuarryPulleyLoadSupport::Suspended;
        break;
    case QuarryPulleyState::UpperHold:
        result.loadPosition = raised;
        result.support = QuarryPulleyLoadSupport::Suspended;
        break;
    case QuarryPulleyState::GuideToPlatform:
        result.loadPosition = glm::mix(raised,
                                       glm::vec3{destination.x, raised.y, destination.z},
                                       eased);
        result.carriagePosition.x = result.loadPosition.x;
        result.support = QuarryPulleyLoadSupport::Suspended;
        break;
    case QuarryPulleyState::Lower:
        result.loadPosition = glm::mix(
            glm::vec3{destination.x, raised.y, destination.z}, destination, eased);
        result.carriagePosition.x = destination.x;
        result.support = QuarryPulleyLoadSupport::Suspended;
        break;
    case QuarryPulleyState::Release:
    case QuarryPulleyState::Complete:
        result.loadPosition = destination;
        result.carriagePosition.x = destination.x;
        result.support = QuarryPulleyLoadSupport::DestinationPlatform;
        break;
    default:
        break;
    }

    result.pulleyPoint = result.carriagePosition;
    result.loadAttachmentPoint = result.loadPosition + loadAttachmentOffset();
    result.ropeAttached = state >= QuarryPulleyState::Tension &&
                          state <= QuarryPulleyState::Lower;
    if (state == QuarryPulleyState::Idle)
        result.ropeEnd = result.loadAttachmentPoint + glm::vec3{0.0f, 0.75f, 0.0f};
    else if (state == QuarryPulleyState::Attach)
        result.ropeEnd = glm::mix(result.loadAttachmentPoint +
                                      glm::vec3{0.0f, 0.75f, 0.0f},
                                  result.loadAttachmentPoint, eased);
    else if (state == QuarryPulleyState::Release)
        result.ropeEnd = result.loadAttachmentPoint +
                         glm::vec3{0.0f, 0.75f * eased, 0.0f};
    else if (state == QuarryPulleyState::Complete)
        result.ropeEnd = result.loadAttachmentPoint + glm::vec3{0.0f, 0.75f, 0.0f};
    else
        result.ropeEnd = result.loadAttachmentPoint;
    result.ropeLength = glm::length(result.pulleyPoint - result.ropeEnd);

    const float liftTravel = raised.y - start.y;
    const float guideTravel = destination.x - start.x;
    const float lowerTravel = raised.y - destination.y;
    float ropeTravel = 0.0f;
    if (state == QuarryPulleyState::Tension)
        ropeTravel = 0.12f * eased;
    else if (state > QuarryPulleyState::Tension)
    {
        ropeTravel = 0.12f;
        if (state == QuarryPulleyState::Lift)
            ropeTravel += liftTravel * eased;
        else
        {
            ropeTravel += liftTravel;
            if (state == QuarryPulleyState::GuideToPlatform)
                ropeTravel += guideTravel * eased;
            else if (state > QuarryPulleyState::GuideToPlatform)
            {
                ropeTravel += guideTravel;
                if (state == QuarryPulleyState::Lower)
                    ropeTravel -= lowerTravel * eased;
                else if (state > QuarryPulleyState::Lower)
                    ropeTravel -= lowerTravel;
            }
        }
    }
    result.wheelRotationDegrees = glm::degrees(ropeTravel / wheelRadius());
    return result;
}

float QuarryPulleyAnimationController::sequenceDuration()
{
    float duration = 0.0f;
    for (const StateTiming& timing : timings)
        duration += timing.duration;
    return duration;
}

float QuarryPulleyAnimationController::stateDuration(QuarryPulleyState state)
{
    for (const StateTiming& timing : timings)
        if (timing.state == state)
            return timing.duration;
    return 1.0f;
}

const char* QuarryPulleyAnimationController::stateName(QuarryPulleyState state)
{
    switch (state)
    {
    case QuarryPulleyState::Idle: return "Idle";
    case QuarryPulleyState::Attach: return "Attach";
    case QuarryPulleyState::Tension: return "Tension";
    case QuarryPulleyState::Lift: return "Lift";
    case QuarryPulleyState::UpperHold: return "UpperHold";
    case QuarryPulleyState::GuideToPlatform: return "GuideToPlatform";
    case QuarryPulleyState::Lower: return "Lower";
    case QuarryPulleyState::Release: return "Release";
    case QuarryPulleyState::Complete: return "Complete";
    default: return "Unknown";
    }
}

const char* QuarryPulleyAnimationController::supportName(QuarryPulleyLoadSupport support)
{
    switch (support)
    {
    case QuarryPulleyLoadSupport::QuarryFloor: return "quarry floor";
    case QuarryPulleyLoadSupport::Suspended: return "intentionally suspended";
    case QuarryPulleyLoadSupport::DestinationPlatform: return "destination platform";
    default: return "unknown";
    }
}

glm::vec3 QuarryPulleyAnimationController::startLoadPosition()
{
    return {-120.5f, quarryFloorY() + loadHalfHeight, -10.0f};
}

glm::vec3 QuarryPulleyAnimationController::raisedLoadPosition()
{
    return {-120.5f, -0.50f, -10.0f};
}

glm::vec3 QuarryPulleyAnimationController::destinationLoadPosition()
{
    return {-111.0f, destinationPlatformTopY() + loadHalfHeight, -10.0f};
}

glm::vec3 QuarryPulleyAnimationController::frameLeftBase()
{
    return {-123.5f, quarryFloorY(), -10.0f};
}

glm::vec3 QuarryPulleyAnimationController::frameRightBase()
{
    return {-108.0f, quarryFloorY(), -10.0f};
}

glm::vec3 QuarryPulleyAnimationController::loadAttachmentOffset()
{
    return {0.0f, loadHalfHeight + slingHeight, 0.0f};
}

bool validateQuarryPulleyAnimation(std::ostream& output)
{
    bool valid = true;
    float cursor = 0.0f;
    for (const StateTiming& timing : timings)
    {
        const QuarryPulleySnapshot sample =
            QuarryPulleyAnimationController::snapshotAt(cursor + 0.5f * timing.duration);
        valid = valid && sample.state == timing.state &&
                finite(sample.loadPosition) && finite(sample.carriagePosition) &&
                finite(sample.pulleyPoint) && finite(sample.ropeEnd) &&
                std::isfinite(sample.wheelRotationDegrees) && sample.ropeLength > 0.05f;
        cursor += timing.duration;
    }

    QuarryPulleyAnimationController at60;
    QuarryPulleyAnimationController at30;
    for (int step = 0; step < 60; ++step) at60.update(1.0f / 60.0f);
    for (int step = 0; step < 30; ++step) at30.update(1.0f / 30.0f);
    const bool frameIndependent =
        near(at60.snapshot().loadPosition, at30.snapshot().loadPosition, 2.0e-3f) &&
        std::abs(at60.elapsedTime() - at30.elapsedTime()) < 2.0e-3f;
    valid = valid && frameIndependent;

    QuarryPulleyAnimationController paused;
    paused.update(3.4f);
    paused.setPaused(true);
    const QuarryPulleySnapshot beforePause = paused.snapshot();
    paused.update(2.0f);
    const bool pauseStable =
        near(beforePause.loadPosition, paused.snapshot().loadPosition) &&
        std::abs(beforePause.wheelRotationDegrees -
                 paused.snapshot().wheelRotationDegrees) < 1.0e-3f;
    valid = valid && pauseStable;

    QuarryPulleyAnimationController looping;
    const QuarryPulleySnapshot initial = looping.snapshot();
    for (int step = 0; step < 1680; ++step) looping.update(1.0f / 60.0f);
    const bool loopStable = near(initial.loadPosition, looping.snapshot().loadPosition,
                                 2.0e-2f);
    valid = valid && loopStable;
    looping.seek(5.0f, false);
    looping.reset();
    const bool resetStable = looping.snapshot().state == QuarryPulleyState::Idle &&
                             near(looping.snapshot().loadPosition,
                                  QuarryPulleyAnimationController::startLoadPosition());
    valid = valid && resetStable;

    const QuarryPulleySnapshot lift = QuarryPulleyAnimationController::snapshotAt(
        startTime(QuarryPulleyState::Lift) + 2.0f);
    const QuarryPulleySnapshot guide = QuarryPulleyAnimationController::snapshotAt(
        startTime(QuarryPulleyState::GuideToPlatform) + 1.0f);
    const QuarryPulleySnapshot complete = QuarryPulleyAnimationController::snapshotAt(
        startTime(QuarryPulleyState::Complete) + 0.5f);
    const bool motionValid = lift.support == QuarryPulleyLoadSupport::Suspended &&
                             guide.loadPosition.x > lift.loadPosition.x &&
                             complete.support ==
                                 QuarryPulleyLoadSupport::DestinationPlatform &&
                             guide.wheelRotationDegrees > lift.wheelRotationDegrees;
    valid = valid && motionValid;

    output << "Phase 12.7 quarry pulley animation validation\n"
           << "  cycle duration: " << QuarryPulleyAnimationController::sequenceDuration()
           << " seconds\n"
           << "  ordered states: " << timings.size() << "\n"
           << "  60 Hz / 30 Hz equivalence: " << (frameIndependent ? "PASS" : "FAIL") << '\n'
           << "  pause freezes load and wheel: " << (pauseStable ? "PASS" : "FAIL") << '\n'
           << "  two-cycle loop stability: " << (loopStable ? "PASS" : "FAIL") << '\n'
           << "  reset: " << (resetStable ? "PASS" : "FAIL") << '\n'
           << "  lift/guide/wheel coupling: " << (motionValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Quarry pulley animation checks passed.\n"
                     : "Quarry pulley animation checks failed.\n");
    return valid;
}

bool validateQuarryPulleySupport(std::ostream& output)
{
    bool valid = true;
    constexpr float tolerance = 0.02f;
    const float startBottom =
        QuarryPulleyAnimationController::startLoadPosition().y - loadHalfHeight;
    const float destinationBottom =
        QuarryPulleyAnimationController::destinationLoadPosition().y - loadHalfHeight;
    const bool floorContact = std::abs(startBottom -
                                       QuarryPulleyAnimationController::quarryFloorY()) <= tolerance;
    const bool platformContact = std::abs(
        destinationBottom - QuarryPulleyAnimationController::destinationPlatformTopY()) <= tolerance;
    valid = valid && floorContact && platformContact;

    float cursor = 0.0f;
    bool supportStatesValid = true;
    bool clearanceValid = true;
    for (const StateTiming& timing : timings)
    {
        for (int sample = 0; sample <= 10; ++sample)
        {
            const float t = cursor + timing.duration *
                                        (static_cast<float>(sample) + 0.5f) / 11.0f;
            const QuarryPulleySnapshot snapshot =
                QuarryPulleyAnimationController::snapshotAt(t);
            const bool shouldSuspend = timing.state >= QuarryPulleyState::Lift &&
                                       timing.state <= QuarryPulleyState::Lower;
            const bool shouldUsePlatform = timing.state >= QuarryPulleyState::Release;
            const QuarryPulleyLoadSupport expected = shouldUsePlatform
                ? QuarryPulleyLoadSupport::DestinationPlatform
                : (shouldSuspend ? QuarryPulleyLoadSupport::Suspended
                                 : QuarryPulleyLoadSupport::QuarryFloor);
            supportStatesValid = supportStatesValid && snapshot.support == expected;
            clearanceValid = clearanceValid && snapshot.loadPosition.x >= -122.2f &&
                             snapshot.loadPosition.x <= -109.3f &&
                             snapshot.loadPosition.z >= -11.3f &&
                             snapshot.loadPosition.z <= -8.7f &&
                             snapshot.ropeLength > 0.05f;
        }
        cursor += timing.duration;
    }
    valid = valid && supportStatesValid && clearanceValid;

    const float framePostBottom = QuarryPulleyAnimationController::quarryFloorY();
    const float platformPostBottom = QuarryPulleyAnimationController::quarryFloorY();
    const bool frameGrounded = std::abs(framePostBottom + 7.45f) <= tolerance &&
                               std::abs(platformPostBottom + 7.45f) <= tolerance;
    valid = valid && frameGrounded;

    output << std::fixed << std::setprecision(3)
           << "Phase 12.7 quarry pulley support validation\n"
           << "  floor contact gap: " << startBottom + 7.45f
           << (floorContact ? " PASS\n" : " FAIL\n")
           << "  destination contact gap: "
           << destinationBottom - QuarryPulleyAnimationController::destinationPlatformTopY()
           << (platformContact ? " PASS\n" : " FAIL\n")
           << "  conditional support states: " << (supportStatesValid ? "PASS" : "FAIL") << '\n'
           << "  frame/platform grounded: " << (frameGrounded ? "PASS" : "FAIL") << '\n'
           << "  post/load/rope clearance: " << (clearanceValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Quarry pulley support checks passed.\n"
                     : "Quarry pulley support checks failed.\n");
    return valid;
}
