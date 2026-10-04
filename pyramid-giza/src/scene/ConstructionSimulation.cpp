#include "scene/ConstructionSimulation.h"
#include <algorithm>
#include <cmath>
#include <ostream>
#include <glm/gtc/matrix_transform.hpp>
#include "scene/PyramidInterior.h"

ConstructionSimulation::ConstructionSimulation() = default;

void ConstructionSimulation::initialize(const std::vector<PyramidBlockPlacement>& fullLayout)
{
    layout = fullLayout;
    targetCellsBuildable_.assign(layout.size(), false);
    buildableTargets_ = 0;
    for (std::size_t index = 0; index < layout.size(); ++index)
    {
        targetCellsBuildable_[index] =
            !PyramidInterior::blockIntersectsVoid(layout[index]);
        if (targetCellsBuildable_[index])
            ++buildableTargets_;
    }
    restorePrebuiltBaseline();
}

void ConstructionSimulation::reset()
{
    restorePrebuiltBaseline();
}

void ConstructionSimulation::restorePrebuiltBaseline()
{
    targetCellsOccupied_.assign(layout.size(), false);
    targetCellsPrebuilt_.assign(layout.size(), false);
    prebuiltBlocks_ = 0;
    for (std::size_t index = 0; index < layout.size(); ++index)
    {
        const bool prebuilt = targetCellsBuildable_[index] &&
                              layout[index].level < prebuiltLevelCount;
        targetCellsOccupied_[index] = prebuilt;
        targetCellsPrebuilt_[index] = prebuilt;
        if (prebuilt)
            ++prebuiltBlocks_;
    }
    activeBlocks.clear();
    runtimeSettledBlocks_ = 0;
    nextTargetIndex_ = 0;
    nextBlockId_ = 1000;
    ++occupancyRevision_;
}

void ConstructionSimulation::update(float deltaTime)
{
    // Update individual block generic physics / simulation here if needed
}

ConstructionBlock* ConstructionSimulation::getBlock(uint64_t id)
{
    for (auto& block : activeBlocks)
    {
        if (block.id == id) return &block;
    }
    return nullptr;
}

void ConstructionSimulation::spawnNewBlock(const glm::vec3& quarryPos)
{
    ConstructionBlock block;
    block.id = nextBlockId_++;
    block.state = BlockState::QuarryBedrock;
    block.position = quarryPos;
    block.previousPosition = quarryPos;
    
    // Assign a target immediately upon creation so we know where it's going
    if (!assignTarget(block))
        return;

    activeBlocks.push_back(block);
}

bool ConstructionSimulation::assignTarget(ConstructionBlock& block)
{
    while (nextTargetIndex_ < layout.size() &&
           (targetCellsOccupied_[nextTargetIndex_] ||
            !targetCellsBuildable_[nextTargetIndex_]))
        ++nextTargetIndex_;
    if (nextTargetIndex_ >= layout.size())
        return false;

    block.targetIndex = nextTargetIndex_;
    block.targetPlacement = layout[nextTargetIndex_];
    ++nextTargetIndex_;
    return true;
}

bool ConstructionSimulation::isTargetOccupied(std::size_t index) const
{
    return index < targetCellsOccupied_.size() && targetCellsOccupied_[index];
}

bool ConstructionSimulation::isTargetPrebuilt(std::size_t index) const
{
    return index < targetCellsPrebuilt_.size() && targetCellsPrebuilt_[index];
}

bool ConstructionSimulation::isTargetBuildable(std::size_t index) const
{
    return index < targetCellsBuildable_.size() && targetCellsBuildable_[index];
}

bool ConstructionSimulation::settleBlock(uint64_t blockId)
{
    ConstructionBlock* block = getBlock(blockId);
    if (block == nullptr || block->targetIndex >= targetCellsOccupied_.size() ||
        !targetCellsBuildable_[block->targetIndex] ||
        targetCellsOccupied_[block->targetIndex])
        return false;

    targetCellsOccupied_[block->targetIndex] = true;
    ++runtimeSettledBlocks_;
    ++occupancyRevision_;
    block->state = BlockState::Settled;
    block->position = block->targetPlacement.position;
    block->previousPosition = block->position;
    block->rotation = glm::vec3{0.0f};
    block->scale = block->targetPlacement.scale;
    block->taskProgress = 1.0f;
    block->isSettled = true;
    return true;
}

void ConstructionSimulation::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    for (const auto& block : activeBlocks)
    {
        if (block.state == BlockState::QuarryBedrock || block.state == BlockState::Settled)
            continue; // Bedrock is a quarry deposit; settled blocks use occupied target batches.

        MaterialId material = MaterialId::PreparedStone;
        if (block.state == BlockState::Extracting || block.state == BlockState::Separating) {
            material = MaterialId::QuarryStone;
        } else if (block.state == BlockState::Shaping) {
            material = (block.taskProgress > 0.5f) ? MaterialId::LimestoneVariation : MaterialId::QuarryStone;
        } else {
            material = MaterialId::PreparedStone;
        }

        // Apply scale/transform
        glm::mat4 model = glm::translate(glm::mat4(1.0f), block.position);
        
        // Logistics and scene transforms express Euler angles in degrees.
        if (glm::length(block.rotation) > 0.001f) {
            model = glm::rotate(model, glm::radians(block.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(block.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(block.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        }

        const glm::mat4 scaleOnly = glm::scale(glm::mat4(1.0f), block.scale);

        objects.push_back({ScenePrimitive::Cube, model * scaleOnly, material});
    }
}

bool validateConstructionOccupancy(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> completeLayout =
        PyramidLayout::generateComplete(config);
    ConstructionSimulation simulation;
    simulation.initialize(completeLayout);

    const std::size_t baseline = simulation.prebuiltCount();
    const InteriorExclusionStats interiorStats =
        PyramidInterior::exclusionStats(completeLayout);
    const InteriorBounds interiorBounds = PyramidInterior::bounds();
    const float baselineTop = config.origin.y +
        static_cast<float>(ConstructionSimulation::prebuiltLevelCount) *
            config.blockHeight;
    const bool interiorSupported =
        interiorStats.maximumLevel < ConstructionSimulation::prebuiltLevelCount &&
        interiorBounds.maximum.y <= baselineTop + 1.0e-5f;
    const bool baselineCountValid = baseline > 0 &&
        simulation.occupiedTargetCount() == baseline &&
        simulation.runtimeSettledCount() == 0 &&
        simulation.buildableTargetCount() == 7459u;
    bool baselineRuleValid = true;
    bool unbuiltTargetsEmpty = true;
    bool transformsFinite = true;
    bool entranceOpen = true;
    for (std::size_t index = 0; index < simulation.layout.size(); ++index)
    {
        const PyramidBlockPlacement& target = simulation.layout[index];
        const bool expectedPrebuilt = simulation.isTargetBuildable(index) &&
            target.level < ConstructionSimulation::prebuiltLevelCount;
        baselineRuleValid = baselineRuleValid &&
            simulation.isTargetPrebuilt(index) == expectedPrebuilt &&
            simulation.isTargetOccupied(index) == expectedPrebuilt;
        if (target.level >= ConstructionSimulation::prebuiltLevelCount &&
            simulation.isTargetBuildable(index))
            unbuiltTargetsEmpty = unbuiltTargetsEmpty &&
                                  !simulation.isTargetOccupied(index);
        transformsFinite = transformsFinite &&
            std::isfinite(target.position.x) &&
            std::isfinite(target.position.y) &&
            std::isfinite(target.position.z) &&
            std::isfinite(target.scale.x) &&
            std::isfinite(target.scale.y) &&
            std::isfinite(target.scale.z);
        if (PyramidInterior::isEntranceOpeningBlock(target))
            entranceOpen = entranceOpen &&
                           !simulation.isTargetBuildable(index) &&
                           !simulation.isTargetOccupied(index);
    }

    simulation.spawnNewBlock({-143.0f, -6.75f, -22.0f});
    ConstructionBlock* first = simulation.activeBlocks.empty()
                                   ? nullptr
                                   : &simulation.activeBlocks.back();
    const std::size_t firstTarget = first == nullptr
                                        ? simulation.totalCount()
                                        : first->targetIndex;
    const std::size_t unrelatedTarget = firstTarget + 1;
    const bool firstWasEmpty = first != nullptr &&
                               simulation.isTargetBuildable(firstTarget) &&
                               !simulation.isTargetOccupied(firstTarget) &&
                               !simulation.isTargetPrebuilt(firstTarget) &&
                               first->targetPlacement.level ==
                                   ConstructionSimulation::prebuiltLevelCount;
    const bool firstSettled = first != nullptr && simulation.settleBlock(first->id);
    const bool exactFirstOccupied = simulation.isTargetOccupied(firstTarget);
    const bool unrelatedStillEmpty =
        unrelatedTarget < simulation.totalCount() &&
        !simulation.isTargetOccupied(unrelatedTarget);
    const bool duplicateRejected =
        first != nullptr && !simulation.settleBlock(first->id);

    simulation.spawnNewBlock({-143.0f, -6.75f, -22.0f});
    ConstructionBlock* second = simulation.activeBlocks.empty()
                                    ? nullptr
                                    : &simulation.activeBlocks.back();
    const bool distinctTarget = second != nullptr &&
                                second->targetIndex != firstTarget;
    const bool secondSettled = second != nullptr &&
                               simulation.settleBlock(second->id);
    const bool twoOccupied = simulation.occupiedTargetCount() == baseline + 2 &&
                             simulation.runtimeSettledCount() == 2;

    simulation.spawnNewBlock({-143.0f, -6.75f, -22.0f});
    ConstructionBlock* invalid = simulation.activeBlocks.empty()
                                     ? nullptr
                                     : &simulation.activeBlocks.back();
    if (invalid != nullptr)
        invalid->targetIndex = simulation.totalCount();
    const bool invalidRejected = invalid != nullptr &&
                                 !simulation.settleBlock(invalid->id);

    simulation.reset();
    const bool resetValid = simulation.occupiedTargetCount() == baseline &&
                            simulation.prebuiltCount() == baseline &&
                            simulation.runtimeSettledCount() == 0 &&
                            simulation.activeBlocks.empty() &&
                            !simulation.isTargetOccupied(firstTarget);

    const bool valid = baselineCountValid && baselineRuleValid &&
                       interiorSupported &&
                       unbuiltTargetsEmpty && transformsFinite && entranceOpen &&
                       firstWasEmpty && firstSettled &&
                       exactFirstOccupied && unrelatedStillEmpty &&
                       duplicateRejected && distinctTarget && secondSettled &&
                       twoOccupied && invalidRejected && resetValid;
    output << "Phase 13 authoritative prebuilt occupancy validation\n"
           << "  12-course structural baseline / buildable targets: "
           << baseline << " / " << simulation.buildableTargetCount() << ' '
           << (baselineCountValid && baselineRuleValid ? "PASS" : "FAIL") << '\n'
           << "  interior voids reserved and entrance remains open: "
           << (entranceOpen ? "PASS" : "FAIL") << '\n'
           << "  interior through level " << interiorStats.maximumLevel
           << " enclosed below baseline top y=" << baselineTop << ": "
           << (interiorSupported ? "PASS" : "FAIL") << '\n'
           << "  upper targets unoccupied and transforms finite: "
           << (unbuiltTargetsEmpty && transformsFinite ? "PASS" : "FAIL") << '\n'
           << "  exact first target occupied only: "
           << (firstSettled && exactFirstOccupied && unrelatedStillEmpty
                   ? "PASS" : "FAIL") << '\n'
           << "  duplicate and invalid targets rejected: "
           << (duplicateRejected && invalidRejected ? "PASS" : "FAIL") << '\n'
           << "  second target raises runtime occupied count to two: "
           << (distinctTarget && secondSettled && twoOccupied ? "PASS" : "FAIL")
           << '\n'
           << "  reset restores baseline and clears runtime state: "
           << (resetValid ? "PASS" : "FAIL") << '\n';
    return valid;
}
