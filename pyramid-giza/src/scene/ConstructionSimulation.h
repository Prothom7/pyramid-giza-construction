#pragma once

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <vector>
#include <glm/glm.hpp>
#include "scene/PyramidLayout.h"
#include "scene/SceneTypes.h"

enum class BlockState
{
    QuarryBedrock,
    Extracting,
    Separating,
    Shaping,
    Staged,
    LoadedOnSledge,
    Hauling,
    RampApproach,
    RampAscent,
    LiftPrep,
    Lifting,
    UpperStaging,
    Placement,
    Settling,
    Settled
};

struct ConstructionBlock
{
    uint64_t id = 0;
    BlockState state = BlockState::QuarryBedrock;

    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{2.6f, 1.6f, 2.4f};

    int sledgeId = -1;
    PyramidBlockPlacement targetPlacement;
    std::size_t targetIndex = std::numeric_limits<std::size_t>::max();

    float taskProgress = 0.0f; // Generic progress for current state

    bool isExtracted = false;
    bool isShaped = false;
    bool isLoaded = false;
    bool isSettled = false;
};

class ConstructionSimulation
{
public:
    // Courses 0-11 enclose every Phase 12.8 interior void (maximum level 11)
    // while leaving the upper 16 courses for authoritative physical work.
    static constexpr unsigned int prebuiltLevelCount = 12u;

    ConstructionSimulation();

    void initialize(const std::vector<PyramidBlockPlacement>& fullLayout);
    void update(float deltaTime);
    void reset();

    // Central authoritative data
    std::vector<ConstructionBlock> activeBlocks;

    void collectSceneObjects(std::vector<SceneObject>& objects) const;
    std::vector<PyramidBlockPlacement> layout;

    // settledCount is deliberately the number constructed during this run.
    // Prebuilt and runtime stones share target occupancy/rendering, but remain
    // separate for reset and construction accounting.
    std::size_t settledCount() const { return runtimeSettledBlocks_; }
    std::size_t runtimeSettledCount() const { return runtimeSettledBlocks_; }
    std::size_t prebuiltCount() const { return prebuiltBlocks_; }
    std::size_t occupiedTargetCount() const
    {
        return prebuiltBlocks_ + runtimeSettledBlocks_;
    }
    std::size_t totalCount() const { return targetCellsOccupied_.size(); }
    std::size_t buildableTargetCount() const { return buildableTargets_; }
    std::uint64_t occupancyRevision() const { return occupancyRevision_; }
    bool isTargetOccupied(std::size_t index) const;
    bool isTargetPrebuilt(std::size_t index) const;
    bool isTargetBuildable(std::size_t index) const;
    bool settleBlock(uint64_t blockId);

    // Helpers
    ConstructionBlock* getBlock(uint64_t id);
    void spawnNewBlock(const glm::vec3& quarryPos);
    
    // Assigns an unassigned target to a block
    bool assignTarget(ConstructionBlock& block);

private:
    void restorePrebuiltBaseline();

    std::vector<bool> targetCellsOccupied_; // Indexed by PyramidLayout target index.
    std::vector<bool> targetCellsPrebuilt_;
    std::vector<bool> targetCellsBuildable_;
    std::size_t prebuiltBlocks_ = 0;
    std::size_t runtimeSettledBlocks_ = 0;
    std::size_t buildableTargets_ = 0;
    std::size_t nextTargetIndex_ = 0;
    uint64_t nextBlockId_ = 1000;
    std::uint64_t occupancyRevision_ = 0;
};

bool validateConstructionOccupancy(std::ostream& output);
