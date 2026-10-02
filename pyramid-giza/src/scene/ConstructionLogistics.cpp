#include "scene/ConstructionLogistics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionAnimation.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"
#include <iostream>

namespace
{
constexpr glm::vec3 cargoSocketOffset{0.0f, 1.36f, -0.02f};

glm::vec3 pulleySledgeParkPosition()
{
    const glm::vec3 pickup =
        QuarryPulleyAnimationController::startLoadPosition();
    return {pickup.x - 3.6f,
            QuarryPulleyAnimationController::quarryFloorY(), pickup.z};
}

glm::vec3 pulleyMountedCargoPosition()
{
    return pulleySledgeParkPosition() + cargoSocketOffset;
}

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
    activeBlockId_ = 0;
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
    case LogisticsState::QuarryReady: return "Quarry Ready";
    case LogisticsState::Extracting: return "Extracting Block";
    case LogisticsState::Staged: return "Block Staged";
    case LogisticsState::SledgeLoading: return "Loading Sledge";
    case LogisticsState::Hauling: return "Hauling Sledge";
    case LogisticsState::RampApproach: return "Approaching Ramp";
    case LogisticsState::RampAscent: return "Ramp Ascent";
    case LogisticsState::LiftPrep: return "Lift Preparation";
    case LogisticsState::Lifting: return "Pulley Lift -> Working Tier Elevation";
    case LogisticsState::UpperStaging: return "Upper Tier Staging";
    case LogisticsState::Placement: return "Maneuvering to Target Cell";
    case LogisticsState::Settled: return "Block Settled -> Permanent Structure";
    default: return "Unknown";
    }
}

const char* ConstructionLogistics::routeDescription() const
{
    switch (state_)
    {
    case LogisticsState::QuarryReady:
    case LogisticsState::Extracting:
    case LogisticsState::Staged:
    case LogisticsState::SledgeLoading: return "Quarry Extraction Bay C -> Staging Ground";
    case LogisticsState::Hauling: return "Transport Corridor -> Ramp Base";
    case LogisticsState::RampApproach:
    case LogisticsState::RampAscent:
    case LogisticsState::LiftPrep: return "Main Construction Ramp -> Lift Station";
    case LogisticsState::Lifting:
    case LogisticsState::UpperStaging:
    case LogisticsState::Placement: return "Lift Station -> Upper Construction Tier -> Target Cell";
    case LogisticsState::Settled: return "Integrated into Pyramid Core";
    default: return "";
    }
}

LogisticsSnapshot ConstructionLogistics::snapshot() const
{
    LogisticsSnapshot s;
    s.state = state_;
    s.blockIndex = activeBlockNumber_;
    s.stateProgress = stateProgress_;
    s.blockPosition = blockPosition_;
    s.blockRotation = blockRotation_;
    s.sledgePosition = sledgePosition_;
    s.sledgeHeading = sledgeHeading_;
    s.ropeTaut = ropeTaut_;
    s.activeWorkers = activeWorkers_;
    s.liftHeight = liftHeight_;
    s.blockSettled = justSettled_;
    s.routeDescription = routeDescription();
    return s;
}

glm::vec3 ConstructionLogistics::computeHaulPosition(float progress) const
{
    // Staging ground to base of ramp
    const glm::vec3 start{-108.0f, -6.0f, -5.0f};
    const glm::vec3 end{-10.0f, 1.0f, 42.0f};
    return glm::mix(start, end, progress);
}

glm::vec3 ConstructionLogistics::computeRampPosition(float progress) const
{
    // The legacy state name is retained, but the physical route now finishes
    // beside the actual supported quarry pulley rather than an imaginary lift.
    const glm::vec3 rampStart{0.0f, 1.0f, 45.0f};
    const glm::vec3 rampEnd = pulleySledgeParkPosition();
    return glm::mix(rampStart, rampEnd, progress);
}

glm::vec3 ConstructionLogistics::computePlacementPosition(float progress, ConstructionSimulation& simulation) const
{
    // Placement begins at the real pulley lift top. Final target travel remains
    // a later-task concern, but there is no longer a 6.8-unit downward snap.
    const glm::vec3 stagePos =
        QuarryPulleyAnimationController::raisedLoadPosition();
    glm::vec3 frontierPos = stagePos;
    
    if (activeBlockId_ != 0) {
        for (const auto& block : simulation.activeBlocks) {
            if (block.id == activeBlockId_) {
                frontierPos = block.targetPlacement.position;
                break;
            }
        }
    }
    
    return glm::mix(stagePos, frontierPos, progress);
}

void ConstructionLogistics::advanceState(ConstructionSimulation& simulation, QuarrySystem& quarry,
                                        ConstructionTimelineController& timeline,
                                        QuarryPulleyAnimationController& pulley,
                                        bool physicalLiftEnabled)
{
    const LogisticsState oldState = state_;
    stateTimer_ = 0.0f;
    stateProgress_ = 0.0f;

    switch (state_)
    {
    case LogisticsState::QuarryReady:
    {
        for (auto& block : simulation.activeBlocks) {
            if (block.state == BlockState::Staged) {
                activeBlockId_ = block.id;
                activeBlockNumber_ = static_cast<int>(block.id);
                state_ = LogisticsState::Staged;
                activeWorkers_ = 6;
                ropeTaut_ = false;
                quarry.markCurrentDepositTransported();
                break;
            }
        }
        break;
    }

    case LogisticsState::Extracting:
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
        if (physicalLiftEnabled &&
            !pulley.setPhysicalPreparationProgress(activeBlockId_, 0.0f))
            throw std::runtime_error(
                "Failed to attach authoritative block to physical pulley");
        break;

    case LogisticsState::LiftPrep:
        state_ = LogisticsState::Lifting;
        activeWorkers_ = 6;
        ropeTaut_ = true;
        if (physicalLiftEnabled &&
            !pulley.setPhysicalLiftProgress(activeBlockId_, 0.0f))
            throw std::runtime_error(
                "Physical pulley lost the authoritative block before lifting");
        break;

    case LogisticsState::Lifting:
        state_ = LogisticsState::UpperStaging;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.holdPhysicalBlockAtLiftTop(activeBlockId_))
            throw std::runtime_error(
                "Physical pulley failed to retain the block at lift top");
        break;

    case LogisticsState::UpperStaging:
        state_ = LogisticsState::Placement;
        activeWorkers_ = 6;
        ropeTaut_ = false;
        if (physicalLiftEnabled &&
            !pulley.releasePhysicalBlock(activeBlockId_))
            throw std::runtime_error(
                "Physical pulley released an unexpected block identity");
        break;

    case LogisticsState::Placement:
        if (!simulation.settleBlock(activeBlockId_))
            throw std::runtime_error(
                "Construction block settlement rejected for invalid or occupied target");
        state_ = LogisticsState::Settled;
        activeWorkers_ = 4;
        ropeTaut_ = false;
        justSettled_ = true;
        settledFlashTimer_ = 1.5f;
        timeline.registerPhysicalBlockSettlement(simulation);
        break;

    case LogisticsState::Settled:
        state_ = LogisticsState::QuarryReady;
        activeBlockId_ = 0;
        justSettled_ = false;
        break;
    }

    if (state_ != oldState)
    {
        std::cout << "\n[RUNTIME CONSTRUCTION]\n"
                  << "Block " << activeBlockNumber_ << "\n"
                  << "State: " << static_cast<int>(oldState) << " -> " << static_cast<int>(state_) << "\n"
                  << "Block position: (" << blockPosition_.x << ", " << blockPosition_.y << ", " << blockPosition_.z << ")\n"
                  << "Sledge position: (" << sledgePosition_.x << ", " << sledgePosition_.y << ", " << sledgePosition_.z << ")\n"
                  << "Settled count: " << simulation.settledCount() << std::endl;
    }
}

void ConstructionLogistics::update(float deltaTime, ConstructionSimulation& simulation,
                                   QuarrySystem& quarry,
                                   ConstructionTimelineController& timeline,
                                   QuarryPulleyAnimationController& pulley,
                                   bool physicalLiftEnabled)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    const float duration = stateBaseDuration(state_);
    stateTimer_ += deltaTime;
    stateProgress_ = std::min(1.0f, stateTimer_ / duration);

    bool shouldAdvance = false;
    switch (state_) {
        case LogisticsState::QuarryReady:
        case LogisticsState::Extracting:
            shouldAdvance = false;
            break;
        default:
            shouldAdvance = (stateProgress_ >= 1.0f);
            break;
    }

    if (shouldAdvance && state_ != LogisticsState::QuarryReady) {
        advanceState(simulation, quarry, timeline, pulley,
                     physicalLiftEnabled);
        if (state_ == LogisticsState::QuarryReady) {
            return;
        }
    }

    if (settledFlashTimer_ > 0.0f)
        settledFlashTimer_ = std::max(0.0f, settledFlashTimer_ - deltaTime);

    switch (state_)
    {
    case LogisticsState::QuarryReady:
    {
        advanceState(simulation, quarry, timeline, pulley,
                     physicalLiftEnabled);
        break;
    }
    
    case LogisticsState::Extracting:
    case LogisticsState::Staged:
        break;

    case LogisticsState::SledgeLoading:
    {
        sledgePosition_ = glm::vec3{-108.0f, -6.0f, -5.0f};
        sledgeHeading_ = 0.0f;
        const glm::vec3 groundStaging{-108.0f, -5.2f, -5.0f}; // Resting on ground (Y=-6.0 + 0.8)
        const glm::vec3 mountedOnSledge = sledgePosition_ + cargoSocketOffset;
        blockPosition_ = glm::mix(groundStaging, mountedOnSledge, stateProgress_);
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::Hauling:
    {
        sledgePosition_ = computeHaulPosition(stateProgress_);
        sledgeHeading_ = 0.0f;
        blockPosition_ = sledgePosition_ + glm::vec3(glm::rotate(glm::mat4(1.0f), sledgeHeading_, glm::vec3(0,1,0)) * glm::vec4(0.0f, 1.36f, -0.02f, 1.0f));
        blockRotation_ = glm::vec3{0.0f, sledgeHeading_, 0.0f};
        break;
    }
    case LogisticsState::RampApproach:
    {
        const glm::vec3 p0 = computeHaulPosition(1.0f);
        const glm::vec3 p1{0.0f, 0.0f, 45.0f};
        sledgePosition_ = glm::mix(p0, p1, stateProgress_);
        const float dx = p1.x - p0.x;
        const float dz = p1.z - p0.z;
        sledgeHeading_ = std::atan2(dx, dz);
        blockPosition_ = sledgePosition_ + glm::vec3(glm::rotate(glm::mat4(1.0f), sledgeHeading_, glm::vec3(0,1,0)) * glm::vec4(0.0f, 1.36f, -0.02f, 1.0f));
        blockRotation_ = glm::vec3{0.0f, sledgeHeading_, 0.0f};
        break;
    }
    case LogisticsState::RampAscent:
    {
        sledgePosition_ = computeRampPosition(stateProgress_);
        sledgeHeading_ = 0.0f;
        blockRotation_ = glm::vec3{-0.15f, sledgeHeading_, 0.0f};
        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), sledgeHeading_, glm::vec3(0,1,0));
        rot = glm::rotate(rot, -0.15f, glm::vec3(1,0,0));
        blockPosition_ = sledgePosition_ + glm::vec3(rot * glm::vec4(0.0f, 1.36f, -0.02f, 1.0f));
        break;
    }
    case LogisticsState::LiftPrep:
    {
        sledgePosition_ = pulleySledgeParkPosition();
        sledgeHeading_ = 0.0f;
        if (physicalLiftEnabled)
        {
            if (!pulley.setPhysicalPreparationProgress(activeBlockId_,
                                                       stateProgress_))
                throw std::runtime_error(
                    "Physical pulley preparation rejected the active block");
            // Finish positioning the stone before the controller reaches its
            // tension phase, so the sling always closes on the real payload.
            const float positioning = glm::smoothstep(
                0.0f, 0.55f, stateProgress_);
            blockPosition_ = glm::mix(
                pulleyMountedCargoPosition(),
                QuarryPulleyAnimationController::startLoadPosition(),
                positioning);
        }
        else
            blockPosition_ = pulleyMountedCargoPosition();
        blockRotation_ = glm::vec3{0.0f};
        break;
    }
    case LogisticsState::Lifting:
    {
        sledgePosition_ = pulleySledgeParkPosition();
        sledgeHeading_ = 0.0f;
        if (physicalLiftEnabled)
        {
            if (!pulley.setPhysicalLiftProgress(activeBlockId_, stateProgress_))
                throw std::runtime_error(
                    "Physical pulley lift rejected the active block");
            const QuarryPulleySnapshot lift = pulley.snapshot();
            blockPosition_ = lift.loadPosition;
            liftHeight_ = blockPosition_.y -
                          QuarryPulleyAnimationController::startLoadPosition().y;
        }
        else
        {
            const float eased = glm::smoothstep(0.0f, 1.0f, stateProgress_);
            blockPosition_ = glm::mix(
                QuarryPulleyAnimationController::startLoadPosition(),
                QuarryPulleyAnimationController::raisedLoadPosition(), eased);
            liftHeight_ = blockPosition_.y -
                          QuarryPulleyAnimationController::startLoadPosition().y;
        }
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    case LogisticsState::UpperStaging:
        sledgePosition_ = pulleySledgeParkPosition();
        blockPosition_ =
            QuarryPulleyAnimationController::raisedLoadPosition();
        blockRotation_ = glm::vec3{0.0f};
        break;

    case LogisticsState::Placement:
        sledgePosition_ = pulleySledgeParkPosition();
        blockPosition_ = computePlacementPosition(stateProgress_, simulation);
        blockRotation_ = glm::vec3{0.0f};
        break;

    case LogisticsState::Settled:
        sledgePosition_ = pulleySledgeParkPosition();
        blockPosition_ = computePlacementPosition(1.0f, simulation);
        blockRotation_ = glm::vec3{0.0f};
        break;
    }

    if (activeBlockId_ != 0) {
        ConstructionBlock* block = simulation.getBlock(activeBlockId_);
        if (block) {
            block->previousPosition = block->position;
            block->position = blockPosition_;
            block->rotation = blockRotation_;
            block->sledgeId = (state_ >= LogisticsState::SledgeLoading && state_ <= LogisticsState::RampAscent) ? 1 : -1;
            block->taskProgress = stateProgress_;
            
            switch (state_) {
                case LogisticsState::SledgeLoading: block->state = BlockState::LoadedOnSledge; break;
                case LogisticsState::Hauling: block->state = BlockState::Hauling; break;
                case LogisticsState::RampApproach: block->state = BlockState::RampApproach; break;
                case LogisticsState::RampAscent: block->state = BlockState::RampAscent; break;
                case LogisticsState::LiftPrep: block->state = BlockState::LiftPrep; break;
                case LogisticsState::Lifting: block->state = BlockState::Lifting; break;
                case LogisticsState::UpperStaging: block->state = BlockState::UpperStaging; break;
                case LogisticsState::Placement: block->state = BlockState::Placement; break;
                case LogisticsState::Settled: block->state = BlockState::Settled; break;
                default: break;
            }
        }
    }
}

namespace
{
bool validatePhysicalPulleyIntegration(std::ostream& output)
{
    ConstructionSimulation simulation;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;

    simulation.initialize(
        PyramidLayout::generateComplete(PyramidLayoutConfig{}));
    timeline.setProgress(0.0f);
    quarry.startExtraction(simulation, 0);
    if (simulation.activeBlocks.empty())
    {
        output << "Phase 13 physical pulley integration validation\n"
               << "  authoritative block creation: FAIL\n";
        return false;
    }

    const std::uint64_t blockId = simulation.activeBlocks.back().id;
    bool pickupReached = false;
    bool attachmentAligned = true;
    bool ropeAligned = true;
    bool sameIdentity = true;
    bool singlePayload = true;
    std::size_t maximumSimulationCubes = 0;
    std::size_t maximumMatchingPayloadCubes = 0;
    bool logisticsDummyDetected = false;
    bool liftSampled = false;
    bool topReached = false;
    float maximumAttachmentError = 0.0f;
    glm::vec3 pickupBlock{0.0f};
    glm::vec3 liftTopBlock{0.0f};

    constexpr float deltaTime = 0.05f;
    constexpr int maximumSteps = 10000;
    for (int step = 0; step < maximumSteps && !topReached; ++step)
    {
        quarry.update(deltaTime, simulation);
        logistics.update(deltaTime, simulation, quarry, timeline, pulley, true);

        ConstructionBlock* block = simulation.getBlock(blockId);
        if (block == nullptr)
        {
            sameIdentity = false;
            break;
        }

        const QuarryPulleySnapshot snapshot = pulley.snapshot();
        if (logistics.state() == LogisticsState::LiftPrep &&
            logistics.stateProgress() >= 0.90f)
        {
            pickupBlock = block->position;
            pickupReached =
                glm::distance(block->position,
                              QuarryPulleyAnimationController::startLoadPosition()) <=
                1.0e-3f;
            sameIdentity = sameIdentity &&
                           pulley.activePhysicalBlockId() == blockId;
        }
        else if (logistics.state() == LogisticsState::Lifting)
        {
            liftSampled = true;
            const glm::vec3 blockAttachment =
                block->position +
                QuarryPulleyAnimationController::loadAttachmentOffset();
            const float attachmentError =
                glm::distance(blockAttachment, snapshot.loadAttachmentPoint);
            maximumAttachmentError =
                std::max(maximumAttachmentError, attachmentError);
            attachmentAligned = attachmentAligned &&
                                glm::distance(block->position,
                                              snapshot.loadPosition) <= 1.0e-3f &&
                                attachmentError <= 1.0e-3f;
            ropeAligned = ropeAligned && snapshot.ropeAttached &&
                          glm::distance(snapshot.ropeEnd,
                                        blockAttachment) <= 1.0e-3f;
            sameIdentity = sameIdentity &&
                           pulley.activePhysicalBlockId() == blockId;

            std::vector<SceneObject> simulationObjects;
            std::vector<SceneObject> logisticsObjects;
            simulation.collectSceneObjects(simulationObjects);
            logistics.collectSceneObjects(logisticsObjects);
            const std::size_t simulationCubes =
                static_cast<std::size_t>(std::count_if(
                    simulationObjects.begin(), simulationObjects.end(),
                    [](const SceneObject& object) {
                        return object.primitive == ScenePrimitive::Cube;
                    }));
            const std::size_t matchingPayloadCubes =
                static_cast<std::size_t>(std::count_if(
                    simulationObjects.begin(), simulationObjects.end(),
                    [block](const SceneObject& object) {
                        return object.primitive == ScenePrimitive::Cube &&
                               glm::distance(glm::vec3{object.model[3]},
                                             block->position) <= 1.0e-3f;
                    }));
            const bool logisticsHasDummy = std::any_of(
                logisticsObjects.begin(), logisticsObjects.end(),
                [](const SceneObject& object) {
                    return object.primitive == ScenePrimitive::Cube;
                });
            maximumSimulationCubes =
                std::max(maximumSimulationCubes, simulationCubes);
            maximumMatchingPayloadCubes = std::max(
                maximumMatchingPayloadCubes, matchingPayloadCubes);
            logisticsDummyDetected =
                logisticsDummyDetected || logisticsHasDummy;
            singlePayload = singlePayload && matchingPayloadCubes == 1 &&
                            !logisticsHasDummy;
        }
        else if (logistics.state() == LogisticsState::UpperStaging)
        {
            liftTopBlock = block->position;
            topReached =
                glm::distance(block->position,
                              QuarryPulleyAnimationController::raisedLoadPosition()) <=
                    1.0e-3f &&
                glm::distance(block->position, snapshot.loadPosition) <=
                    1.0e-3f &&
                pulley.activePhysicalBlockId() == blockId;
        }
    }

    const glm::vec3 sledge = logistics.snapshot().sledgePosition;
    const glm::vec3 pickup =
        QuarryPulleyAnimationController::startLoadPosition();
    const float horizontalSledgeDistance = glm::length(glm::vec2{
        sledge.x - pickup.x, sledge.z - pickup.z});
    const bool sledgeAtRig = horizontalSledgeDistance <= 5.0f;
    const bool valid = pickupReached && liftSampled && attachmentAligned &&
                       ropeAligned && sameIdentity && singlePayload &&
                       topReached && sledgeAtRig;

    output << std::fixed << std::setprecision(3)
           << "Phase 13 physical pulley integration validation\n"
           << "  authoritative block ID: " << blockId << '\n'
           << "  pulley pickup: (" << pickup.x << ", " << pickup.y << ", "
           << pickup.z << ")\n"
           << "  block pickup: (" << pickupBlock.x << ", " << pickupBlock.y
           << ", " << pickupBlock.z << ") "
           << (pickupReached ? "PASS" : "FAIL") << '\n'
           << "  maximum attachment/rope error: "
           << maximumAttachmentError << ' '
           << (attachmentAligned && ropeAligned ? "PASS" : "FAIL") << '\n'
           << "  same block identity throughout lift: "
           << (sameIdentity ? "PASS" : "FAIL") << '\n'
           << "  one matching simulation payload and no logistics dummy: "
           << (singlePayload ? "PASS" : "FAIL")
           << " (matching " << maximumMatchingPayloadCubes
           << ", all active simulation cubes " << maximumSimulationCubes
           << ", logistics dummy "
           << (logisticsDummyDetected ? "YES" : "NO") << ")\n"
           << "  sledge reaches visible pulley area: "
           << (sledgeAtRig ? "PASS" : "FAIL") << '\n'
           << "  lift top: (" << liftTopBlock.x << ", " << liftTopBlock.y
           << ", " << liftTopBlock.z << ") "
           << (topReached ? "PASS" : "FAIL") << '\n'
           << (valid ? "Physical pulley integration checks passed.\n"
                     : "Physical pulley integration checks failed.\n");
    return valid;
}
} // namespace

bool ConstructionLogistics::validateConstructionLogistics(std::ostream& output)
{
    const bool occupancyValid = validateConstructionOccupancy(output);
    const bool pulleyValid = validatePhysicalPulleyIntegration(output);
    return occupancyValid && pulleyValid;
}

bool ConstructionLogistics::validateConstructionTrace(std::ostream& output)
{
    ConstructionSimulation sim;
    QuarrySystem quarry;
    ConstructionTimelineController timeline;
    QuarryPulleyAnimationController pulley;
    ConstructionLogistics logistics;
    
    sim.initialize(PyramidLayout::generate(PyramidLayoutConfig()));
    
    // Disable timeline so simulation only runs due to physical settlement
    timeline.setProgress(0.0f);
    
    quarry.startExtraction(sim, 0);
    uint64_t targetBlockId = sim.activeBlocks.back().id;
    const std::size_t targetIndex = sim.activeBlocks.back().targetIndex;
    const std::size_t unrelatedTarget = targetIndex + 1;
    const bool occupiedBefore = sim.isTargetOccupied(targetIndex);
    
    output << "=== CONSTRUCTION TRACE ===\n\n";
    output << "Block " << targetBlockId << "\n";
    output << "Target Cell Index: " << targetIndex << "\n";
    output << "Occupied Before Settlement: "
           << (occupiedBefore ? "YES" : "NO") << "\n\n";
           
    const int maxSteps = 10000;
    const float dt = 0.1f;
    BlockState currentState = BlockState::QuarryBedrock;
    
    for (int step = 0; step < maxSteps; ++step) {
        quarry.update(dt, sim);
        
        ConstructionBlock* block = sim.getBlock(targetBlockId);
        if (block) {
            BlockState newState = block->state;
            if (newState != currentState) {
                output << (int)currentState << " -> " 
                       << (int)newState << "\n";
                output << "position = (" << block->position.x << ", " << block->position.y << ", " << block->position.z << ")\n";
                output << "target = (" << block->targetPlacement.position.x << ", " << block->targetPlacement.position.y << ", " << block->targetPlacement.position.z << ")\n\n";
                currentState = newState;
            }
        }
        
        logistics.update(dt, sim, quarry, timeline, pulley);
        
        block = sim.getBlock(targetBlockId);
        if (block) {
            BlockState newState = block->state;
            if (newState != currentState) {
                output << (int)currentState << " -> " 
                       << (int)newState << "\n";
                output << "position = (" << block->position.x << ", " << block->position.y << ", " << block->position.z << ")\n";
                output << "target = (" << block->targetPlacement.position.x << ", " << block->targetPlacement.position.y << ", " << block->targetPlacement.position.z << ")\n\n";
                currentState = newState;
            }
            if (newState == BlockState::Settled) {
                break;
            }
        }
    }
    
    ConstructionBlock* block = sim.getBlock(targetBlockId);
    if (!block) {
        output << "Block lost in simulation.\nFAIL\n";
        return false;
    }
    
    output << "Final Position: (" << block->position.x << ", " << block->position.y << ", " << block->position.z << ")\n";
    output << "Target Position: (" << block->targetPlacement.position.x << ", " << block->targetPlacement.position.y << ", " << block->targetPlacement.position.z << ")\n";
    
    float posError = glm::distance(block->position, block->targetPlacement.position);
    output << "Position Error: " << posError << "\n";
    output << "Settled: " << (block->state == BlockState::Settled ? "YES" : "NO") << "\n\n";
    
    output << "Pyramid Settled Count Before: 0\n";
    output << "Pyramid Settled Count After: " << sim.settledCount() << "\n\n";

    const bool occupiedAfter = sim.isTargetOccupied(targetIndex);
    const bool unrelatedOccupied = unrelatedTarget < sim.totalCount() &&
                                   sim.isTargetOccupied(unrelatedTarget);
    output << "Occupied After Settlement: "
           << (occupiedAfter ? "YES" : "NO") << "\n";
    output << "Unrelated Target Occupied: "
           << (unrelatedOccupied ? "YES" : "NO") << "\n\n";

    bool valid = !occupiedBefore && occupiedAfter && !unrelatedOccupied &&
                 (block->state == BlockState::Settled) &&
                 (posError < 0.1f) && (sim.settledCount() == 1) &&
                 (sim.settledCount() == sim.occupiedTargetCount());
    output << "Conservation Accounting Valid: " << (valid ? "YES" : "NO") << "\n\n";
    output << (valid ? "PASS\n" : "FAIL\n");
    
    return valid;
}

void ConstructionLogistics::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    // The authoritative block body is rendered by ConstructionSimulation.
    if (state_ == LogisticsState::Staged || state_ == LogisticsState::SledgeLoading || state_ == LogisticsState::Hauling ||
        state_ == LogisticsState::RampApproach || state_ == LogisticsState::RampAscent ||
        state_ == LogisticsState::LiftPrep || state_ == LogisticsState::Lifting ||
        state_ == LogisticsState::UpperStaging || state_ == LogisticsState::Placement)
    {
        // The main block body is now rendered by ConstructionSimulation::collectSceneObjects
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

        // The quarry pulley renderer owns the only hoisting rope and sling.
    }
}
