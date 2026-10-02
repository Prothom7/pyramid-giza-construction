#pragma once

#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "scene/SceneTypes.h"
#include "graphics/Mesh.h"

enum class SandCellState
{
    StaticSand,
    WindMoved,
    Sliding,
    Accumulating,
    Settled
};

struct SandCell
{
    float height = 0.5f;          // Current sand layer depth (meters)
    float baseElevation = 0.0f;   // Bedrock elevation
    SandCellState state = SandCellState::Settled;
    bool isObstacle = false;      // Pyramid base, wall, cliff
    bool isTrafficRoute = false;  // Sledge haul road, actively swept
    float windExposure = 1.0f;    // Sheltered by structures
};

struct SandSimulationStats
{
    float totalVolume = 0.0f;
    float maxAccumulation = 0.0f;
    float minSandDepth = 0.0f;
    std::size_t slidingCells = 0;
    std::size_t accumulatingCells = 0;
};

class SandSimulation
{
public:
    static constexpr int GridColumns = 129;
    static constexpr int GridRows = 101;
    static constexpr float WorldMinX = -210.0f;
    static constexpr float WorldMaxX = 210.0f;
    static constexpr float WorldMinZ = -210.0f;
    static constexpr float WorldMaxZ = 120.0f;
    static constexpr float CellSizeX =
        (WorldMaxX - WorldMinX) / static_cast<float>(GridColumns - 1);
    static constexpr float CellSizeZ =
        (WorldMaxZ - WorldMinZ) / static_cast<float>(GridRows - 1);
    static constexpr float ReposeThreshold = 0.35f;

    SandSimulation();

    void update(float deltaTime, const glm::vec3& sledgePos = glm::vec3{0.0f});
    void reset();

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void toggleEnabled() { enabled_ = !enabled_; }

    const glm::vec2& windDirection() const { return windDirection_; }
    float windSpeed() const { return windSpeed_; }
    void setWind(const glm::vec2& dir, float speed);

    float sandHeightAt(float worldX, float worldZ) const;
    float terrainHeightAt(float worldX, float worldZ) const;
    static float staticTerrainHeightAt(float worldX, float worldZ);
    static glm::vec3 staticTerrainNormalAt(float worldX, float worldZ);
    SandCellState cellStateAt(float worldX, float worldZ) const;
    const SandCell& cellAt(int x, int z) const { return grid_[gridIndex(x, z)]; }
    SandSimulationStats stats() const;

    // Visual geometry generation (sand drifts, dune patches, track clearance)
    MeshData generateTerrainMesh() const;

    static bool validateSandSimulation(std::ostream& output);

private:
    void initGrid();
    void simulateWindTransport(float deltaTime);
    void simulateReposeRelaxation(float deltaTime);
    void applyTrafficDisturbance(const glm::vec3& sledgePos);
    int gridIndex(int x, int z) const { return z * GridColumns + x; }
    glm::vec2 cellWorldPos(int x, int z) const;
    bool worldToGrid(float worldX, float worldZ, int& outX, int& outZ,
                     float& localX, float& localZ) const;
    float sampleSurface(float worldX, float worldZ, bool sandDepthOnly) const;

    std::vector<SandCell> grid_;
    glm::vec2 windDirection_{-0.707f, 0.707f};
    float windSpeed_ = 4.2f;
    float simulationTimer_ = 0.0f;
    bool enabled_ = true;
    float initialVolume_ = 0.0f;
};
