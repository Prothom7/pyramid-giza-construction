#include "scene/ConstructionLogistics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionAnimation.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"

namespace
{
float stateBaseDuration(LogisticsState state)
{
    switch (state)
    {
    case LogisticsState::QuarryReady: return 2.0f;
    case LogisticsState::Extracting: return 5.0f;
    case LogisticsState::Staged: return 2.5f;
    case LogisticsState::SledgeLoading: return 3.5f;
    case LogisticsState::Hauling: return 7.0f;
    case LogisticsState::RampApproach: return 4.0f;
    case LogisticsState::RampAscent: return 8.0f;
    case LogisticsState::LiftPrep: return 3.0f;
    case LogisticsState::Lifting: return 6.0f;
    case LogisticsState::UpperStaging: return 3.0f;
    case LogisticsState::Placement: return 5.0f;
    case LogisticsState::Settled: return 2.0f;
    default: return 3.0f;
    }
}
} // namespace

ConstructionLogistics::ConstructionLogistics()
{
    reset();
}

void ConstructionLogistics::reset()
{
    state_ = LogisticsState::QuarryReady;
    activeBlockNumber_ = 101;
    stateProgress_ = 0.0f;
    stateTimer_ = 0.0f;
    blockPosition_ = glm::vec3{-108.0f, -6.2f, -5.0f};
    blockRotation_ = glm::vec3{0.0f};
    sledgePosition_ = glm::vec3{-108.0f, -6.2f, -5.0f};
    sledgeHeading_ = 0.0f;
    ropeTaut_ = false;
    activeWorkers_ = 4;
    liftHeight_ = 0.0f;
    justSettled_ = false;
    settledFlashTimer_ = 0.0f;
}

const char* ConstructionLogistics::stateName(LogisticsState state)
{
    switch (state)
    {
    case LogisticsState::QuarryReady: return "QUARRY_READY";
    case LogisticsState::Extracting: return "EXTRACTING";
    case LogisticsState::Staged: return "STAGED";
    case LogisticsState::SledgeLoading: return "SLEDGE_LOADING";
    case LogisticsState::Hauling: return "HAULING";
    case LogisticsState::RampApproach: return "RAMP_APPROACH";
    case LogisticsState::RampAscent: return "RAMP_ASCENT";
    case LogisticsState::LiftPrep: return "LIFT_PREP";
    case LogisticsState::Lifting: return "LIFTING";
    case LogisticsState::UpperStaging: return "UPPER_STAGING";
    case LogisticsState::Placement: return "PLACEMENT";
    case LogisticsState::Settled: return "SETTLED";
    default: return "UNKNOWN";
    }
}

const char* ConstructionLogistics::routeDescription() const
{
    switch (state_)
    {
    case LogisticsState::QuarryReady: return "Quarry Bedrock -> Marking Seam";
    case LogisticsState::Extracting: return "Quarry Cutting Bay -> Separation";
    case LogisticsState::Staged: return "Quarry Floor Staging -> Sledge Berth";
    case LogisticsState::SledgeLoading: return "Staging Area -> Loading on Sledge";
    case LogisticsState::Hauling: return "Quarry Road -> Plateau Haul Lane";
    case LogisticsState::RampApproach: return "Loading Yard -> Ramp Foot";
    case LogisticsState::RampAscent: return "Main Ramp -> Upper Working Platform";
    case LogisticsState::LiftPrep: return "Upper Deck -> Gantry Rig Attachment";
    case LogisticsState::Lifting: return "Pulley Lift -> Working Tier Elevation";
    case LogisticsState::UpperStaging: return "Upper Scaffold -> Placement Frontier";
    case LogisticsState::Placement: return "Lever Alignment -> Course Seating";
    case LogisticsState::Settled: return "Seated in Pyramid -> Course Expansion";
    default: return "Giza Construction Logistics";
    }
}

glm::vec3 ConstructionLogistics::computeHaulPosition(float progress) const
{
    // Path from quarry staging to loading yard
    const glm::vec3 p0{-108.0f, -6.2f, -5.0f};  // Quarry floor
    const glm::vec3 p1{-91.0f, 0.4f, 9.0f};     // Quarry ramp top
    const glm::vec3 p2{-58.0f, 0.0f, 26.0f};    // Dressing depot
    const glm::vec3 p3{-10.0f, 0.0f, 42.0f};    // Loading yard

    if (progress < 0.33f)
    {
        const float t = progress / 0.33f;
        return glm::mix(p0, p1, t);
    }
    else if (progress < 0.66f)
    {
        const float t = (progress - 0.33f) / 0.33f;
        return glm::mix(p1, p2, t);
    }
    else
    {
        const float t = (progress - 0.66f) / 0.34f;
        return glm::mix(p2, p3, t);
    }
}

glm::vec3 ConstructionLogistics::computeRampPosition(float progress) const
{
    // Ramp ascent from base (0, 0, 45) up along incline to upper deck (0, 8.55, -0.8)
    const glm::vec3 pBase{0.0f, 0.0f, 45.0f};
    const glm::vec3 pMid{0.0f, 4.2f, 22.0f};
    const glm::vec3 pTop{0.0f, 8.55f, -0.8f};

    if (progress < 0.5f)
    {
        const float t = progress * 2.0f;
        return glm::mix(pBase, pMid, t);
    }
    else
    {
        const float t = (progress - 0.5f) * 2.0f;
        return glm::mix(pMid, pTop, t);
    }
}

glm::vec3 ConstructionLogistics::computeLiftPosition(float progress) const
{
    // Vertical lift at upper gantry / platform
    const glm::vec3 base{-91.0f, 0.4f, 22.0f};
    const glm::vec3 top{-91.0f, 6.2f, 22.0f};
    return glm::mix(base, top, progress);
}

glm::vec3 ConstructionLogistics::computePlacementPosition(float progress) const
{
    // Upper platform staging to pyramid construction frontier
    const glm::vec3 stagePos{0.0f, 8.55f, -0.8f};
    const glm::vec3 frontierPos{0.0f, 9.60f, -25.0f};
    return glm::mix(stagePos, frontierPos, progress);
}

void ConstructionLogistics::advanceState(QuarrySystem& quarry,
                                        ConstructionTimelineController& timeline,
                                        QuarryPulleyAnimationController& pulley)
{
    stateTimer_ = 0.0f;
    stateProgress_ = 0.0f;

    switch (state_)
    {
    case LogisticsState::QuarryReady:
        state_ = LogisticsState::Extracting;
        quarry.startExtraction();
        activeWorkers_ = 4;
        ropeTaut_ = false;
        break;

    case LogisticsState::Extracting:
        state_ = LogisticsState::Staged;
        activeWorkers_ = 4;
        ropeTaut_ = false;
        break;

    case LogisticsState::Staged:
        state_ = LogisticsState::SledgeLoading;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        break;

    case LogisticsState::SledgeLoading:
        state_ = LogisticsState::Hauling;
        activeWorkers_ = 8;
        ropeTaut_ = true;
        break;

    case LogisticsState::Hauling:
        state_ = LogisticsState::RampApproach;
        activeWorkers_ = 8;
        ropeTaut_ = true;
        break;

    case LogisticsState::RampApproach:
        state_ = LogisticsState::RampAscent;
        activeWorkers_ = 10;
        ropeTaut_ = true;
        break;

    case LogisticsState::RampAscent:
        state_ = LogisticsState::LiftPrep;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        pulley.seek(0.0f, true);
        break;

    case LogisticsState::LiftPrep:
        state_ = LogisticsState::Lifting;
        activeWorkers_ = 6;
        ropeTaut_ = true;
        break;

    case LogisticsState::Lifting:
        state_ = LogisticsState::UpperStaging;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        break;

    case LogisticsState::UpperStaging:
        state_ = LogisticsState::Placement;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        break;

    case LogisticsState::Placement:
        state_ = LogisticsState::Settled;
        activeWorkers_ = 4;
        ropeTaut_ = false;
        justSettled_ = true;
        settledFlashTimer_ = 1.5f;
        // Block settles into pyramid! Contribute to authoritative timeline progress
        timeline.setProgress(std::min(1.0f, timeline.progress() + 0.005f));
        quarry.markCurrentDepositTransported();
        break;

    case LogisticsState::Settled:
        // Cycle completes, start next quarry block
        state_ = LogisticsState::QuarryReady;
        ++activeBlockNumber_;
        justSettled_ = false;
        break;
    }
}

void ConstructionLogistics::update(float deltaTime, QuarrySystem& quarry,
                                   ConstructionTimelineController& timeline,
                                   QuarryPulleyAnimationController& pulley)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    const float duration = stateBaseDuration(state_);
    stateTimer_ += deltaTime;
    stateProgress_ = std::min(1.0f, stateTimer_ / duration);

    if (settledFlashTimer_ > 0.0f)
        settledFlashTimer_ = std::max(0.0f, settledFlashTimer_ - deltaTime);

    // Update positions and visual state based on active stage
    switch (state_)
    {
    case LogisticsState::QuarryReady:
    case LogisticsState::Extracting:
    {
        const QuarryDeposit* dep = quarry.activeDeposit();
        if (dep != nullptr)
        {
            blockPosition_ = dep->position + glm::vec3{0.0f, dep->size.y * 0.5f + dep->separationOffset, 0.0f};
            blockRotation_ = glm::vec3{0.0f};
            sledgePosition_ = dep->stagedPosition;
        }
        if (quarry.isCurrentDepositStaged() && state_ == LogisticsState::Extracting)
            advanceState(quarry, timeline, pulley);
        break;
    }

    case LogisticsState::Staged:
    {
        blockPosition_ = glm::vec3{-108.0f, -5.4f, -5.0f};
        sledgePosition_ = glm::vec3{-108.0f, -6.0f, -5.0f};
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::SledgeLoading:
    {
        // Block slides from staging ground onto sledge deck
        const glm::vec3 groundStaging{-108.0f, -5.4f, -5.0f};
        const glm::vec3 mountedOnSledge{-108.0f, -5.0f, -5.0f};
        blockPosition_ = glm::mix(groundStaging, mountedOnSledge, stateProgress_);
        sledgePosition_ = glm::vec3{-108.0f, -6.0f, -5.0f};
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::Hauling:
    {
        sledgePosition_ = computeHaulPosition(stateProgress_);
        blockPosition_ = sledgePosition_ + glm::vec3{0.0f, 1.0f, 0.0f};
        sledgeHeading_ = (stateProgress_ < 0.33f) ? 45.0f : 15.0f;
        blockRotation_ = glm::vec3{0.0f, sledgeHeading_, 0.0f};
        break;
    }

    case LogisticsState::RampApproach:
    {
        const glm::vec3 p0{-10.0f, 0.0f, 42.0f};
        const glm::vec3 p1{0.0f, 0.0f, 45.0f};
        sledgePosition_ = glm::mix(p0, p1, stateProgress_);
        blockPosition_ = sledgePosition_ + glm::vec3{0.0f, 1.0f, 0.0f};
        sledgeHeading_ = 0.0f;
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::RampAscent:
    {
        sledgePosition_ = computeRampPosition(stateProgress_);
        blockPosition_ = sledgePosition_ + glm::vec3{0.0f, 1.0f, 0.0f};
        // Pitch with ramp slope (~10 degrees)
        sledgeHeading_ = 0.0f;
        blockRotation_ = glm::vec3{-10.5f, 0.0f, 0.0f};
        break;
    }

    case LogisticsState::LiftPrep:
    {
        blockPosition_ = glm::vec3{0.0f, 9.55f, -0.8f};
        sledgePosition_ = glm::vec3{0.0f, 8.55f, -0.8f};
        blockRotation_ = glm::vec3{0.0f};
        liftHeight_ = 0.0f;
        break;
    }

    case LogisticsState::Lifting:
    {
        liftHeight_ = stateProgress_ * 5.8f;
        blockPosition_ = glm::vec3{0.0f, 9.55f + liftHeight_, -0.8f};
        sledgePosition_ = glm::vec3{0.0f, 8.55f, -0.8f};
        blockRotation_ = glm::vec3{0.0f, std::sin(stateProgress_ * 3.14159f) * 4.0f, 0.0f};
        break;
    }

    case LogisticsState::UpperStaging:
    {
        blockPosition_ = glm::vec3{0.0f, 15.35f, -0.8f};
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::Placement:
    {
        blockPosition_ = computePlacementPosition(stateProgress_);
        // Lever tilt during alignment
        const float tilt = std::sin(stateProgress_ * 3.14159f) * 6.0f;
        blockRotation_ = glm::vec3{tilt, 0.0f, 0.0f};
        break;
    }

    case LogisticsState::Settled:
    {
        blockPosition_ = glm::vec3{0.0f, 9.60f, -25.0f};
        blockRotation_ = glm::vec3{0.0f};
        break;
    }
    }

    if (stateProgress_ >= 1.0f)
        advanceState(quarry, timeline, pulley);
}

LogisticsSnapshot ConstructionLogistics::snapshot() const
{
    LogisticsSnapshot snap;
    snap.state = state_;
    snap.blockIndex = activeBlockNumber_;
    snap.stateProgress = stateProgress_;
    snap.blockPosition = blockPosition_;
    snap.blockRotation = blockRotation_;
    snap.sledgePosition = sledgePosition_;
    snap.sledgeHeading = sledgeHeading_;
    snap.ropeTaut = ropeTaut_;
    snap.activeWorkers = activeWorkers_;
    snap.liftHeight = liftHeight_;
    snap.blockSettled = (state_ == LogisticsState::Settled || justSettled_);
    snap.routeDescription = routeDescription();
    return snap;
}

void ConstructionLogistics::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    // If block is currently being hauled, lifted, or placed, render the active block
    if (state_ == LogisticsState::SledgeLoading || state_ == LogisticsState::Hauling ||
        state_ == LogisticsState::RampApproach || state_ == LogisticsState::RampAscent ||
        state_ == LogisticsState::LiftPrep || state_ == LogisticsState::Lifting ||
        state_ == LogisticsState::UpperStaging || state_ == LogisticsState::Placement)
    {
        const glm::vec3 blockSize{2.6f, 1.35f, 2.45f};
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform(blockPosition_, blockRotation_, blockSize),
                           MaterialId::PreparedStone});

        // Sledge ropes / lashings securing the block to sledge
        if (state_ == LogisticsState::Hauling || state_ == LogisticsState::RampAscent)
        {
            for (float zOffset : {-0.7f, 0.7f})
            {
                objects.push_back({ScenePrimitive::Cylinder,
                                   makeTransform(blockPosition_ + glm::vec3{0.0f, 0.68f, zOffset},
                                                 {0.0f, 0.0f, 90.0f}, {0.06f, 2.7f, 0.06f}),
                                   MaterialId::Rope});
            }
        }

        // Pulley lifting sling during lift
        if (state_ == LogisticsState::Lifting)
        {
            // Vertical hoisting cable
            objects.push_back({ScenePrimitive::Cylinder,
                               makeTransform(blockPosition_ + glm::vec3{0.0f, 3.5f, 0.0f},
                                             {}, {0.07f, 6.0f, 0.07f}),
                               MaterialId::Rope});
            // Sling cradle straps
            for (float xOffset : {-1.0f, 1.0f})
            {
                objects.push_back({ScenePrimitive::Cylinder,
                                   makeTransform(blockPosition_ + glm::vec3{xOffset, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, 0.0f}, {0.05f, 1.4f, 0.05f}),
                                   MaterialId::Rope});
            }
        }
    }
}

bool ConstructionLogistics::validateConstructionLogistics(std::ostream& output)
{
    ConstructionLogistics logistics;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;

    bool sequenceValid = true;
    const LogisticsState expectedSequence[] = {
        LogisticsState::QuarryReady,
        LogisticsState::Extracting,
        LogisticsState::Staged,
        LogisticsState::SledgeLoading,
        LogisticsState::Hauling,
        LogisticsState::RampApproach,
        LogisticsState::RampAscent,
        LogisticsState::LiftPrep,
        LogisticsState::Lifting,
        LogisticsState::UpperStaging,
        LogisticsState::Placement,
        LogisticsState::Settled
    };

    const float initialTimelineProgress = timeline.progress();

    for (LogisticsState expected : expectedSequence)
    {
        sequenceValid = sequenceValid && (logistics.state() == expected);
        // Advance past current state
        const float duration = stateBaseDuration(expected);
        const int steps = 25;
        const float dt = (duration + 0.1f) / steps;
        for (int s = 0; s < steps; ++s)
        {
            logistics.update(dt, quarry, timeline, pulley);
            const LogisticsSnapshot snap = logistics.snapshot();
            sequenceValid = sequenceValid && std::isfinite(snap.blockPosition.x) &&
                            std::isfinite(snap.blockPosition.y) &&
                            std::isfinite(snap.blockPosition.z) &&
                            snap.blockPosition.y >= -7.0f; // No falling through world
        }
    }

    const bool progressAdvanced = timeline.progress() > initialTimelineProgress;
    const bool valid = sequenceValid && progressAdvanced;

    output << "Phase 13 Construction Logistics Validation\n"
           << "  all 12 sequential pipeline states traversed: "
           << (sequenceValid ? "PASS" : "FAIL") << '\n'
           << "  block positions finite and grounded across entire route: "
           << (sequenceValid ? "PASS" : "FAIL") << '\n'
           << "  settled block advances pyramid construction timeline: "
           << (progressAdvanced ? "PASS" : "FAIL") << '\n'
           << (valid ? "Construction logistics validation passed.\n"
                     : "Construction logistics validation failed.\n");

    return valid;
}
