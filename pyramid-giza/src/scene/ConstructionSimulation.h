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
    ConstructionSimulation();

    void initialize(const std::vector<PyramidBlockPlacement>& fullLayout);
    void update(float deltaTime);
    void reset();

    // Central authoritative data
    std::vector<ConstructionBlock> activeBlocks;

    void collectSceneObjects(std::vector<SceneObject>& objects) const;
    std::vector<PyramidBlockPlacement> layout;

    std::size_t settledCount() const { return settledBlocks_; }
    std::size_t occupiedTargetCount() const { return settledBlocks_; }
    std::size_t totalCount() const { return targetCellsOccupied_.size(); }
    std::uint64_t occupancyRevision() const { return occupancyRevision_; }
    bool isTargetOccupied(std::size_t index) const;
    bool settleBlock(uint64_t blockId);

    // Helpers
    ConstructionBlock* getBlock(uint64_t id);
    void spawnNewBlock(const glm::vec3& quarryPos);
    
    // Assigns an unassigned target to a block
    bool assignTarget(ConstructionBlock& block);

private:
    std::vector<bool> targetCellsOccupied_; // Indexed by PyramidLayout target index.
    std::size_t settledBlocks_ = 0;
    std::size_t nextTargetIndex_ = 0;
    uint64_t nextBlockId_ = 1000;
    std::uint64_t occupancyRevision_ = 0;
};

bool validateConstructionOccupancy(std::ostream& output);
