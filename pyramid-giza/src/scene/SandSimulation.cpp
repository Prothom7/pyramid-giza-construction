#include "scene/SandSimulation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>



namespace
{
constexpr float initialSandDepth = 0.12f;

float smoothStep(float edge0, float edge1, float value)
{
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float roundedBoxMask(const glm::vec2& point, const glm::vec2& center,
                     const glm::vec2& halfExtents, float feather)
{
    const glm::vec2 outside = glm::max(glm::abs(point - center) - halfExtents,
                                       glm::vec2{0.0f});
    return 1.0f - smoothStep(0.0f, feather, glm::length(outside));
}

float capsuleMask(const glm::vec2& point, const glm::vec2& start,
                  const glm::vec2& end, float radius, float feather)
{
    const glm::vec2 segment = end - start;
    const float denominator = glm::dot(segment, segment);
    const float t = denominator > 1.0e-6f
                        ? std::clamp(glm::dot(point - start, segment) / denominator,
                                     0.0f, 1.0f)
                        : 0.0f;
    const float distance = glm::length(point - (start + t * segment));
    return 1.0f - smoothStep(radius, radius + feather, distance);
}

float baseSurfaceFormula(float worldX, float worldZ)
{
    const glm::vec2 point{worldX, worldZ};
    float height =
        0.30f * std::sin(worldX * 0.018f + 0.45f) *
            std::cos(worldZ * 0.015f - 0.70f) +
        0.12f * std::sin(worldX * 0.041f + worldZ * 0.026f + 1.10f) +
        0.05f * std::cos(worldX * 0.071f - worldZ * 0.053f);

    float flatMask = 0.0f;
    flatMask = std::max(flatMask,
                        roundedBoxMask(point, {0.0f, -42.0f}, {68.0f, 70.0f}, 20.0f));
    flatMask = std::max(flatMask,
                        capsuleMask(point, {-108.0f, -5.0f}, {0.0f, 45.0f},
                                    15.0f, 13.0f));
    flatMask = std::max(flatMask,
                        roundedBoxMask(point, {-55.0f, 25.0f}, {48.0f, 28.0f}, 14.0f));
    flatMask = std::max(flatMask,
                        roundedBoxMask(point, {92.0f, -105.0f}, {24.0f, 20.0f}, 16.0f));
    flatMask = std::max(flatMask,
                        roundedBoxMask(point, {0.0f, -143.0f}, {175.0f, 14.0f}, 10.0f));
    height = glm::mix(height, 0.0f, flatMask);

    const float quarryMask =
        roundedBoxMask(point, {-128.0f, -15.0f}, {21.0f, 17.0f}, 20.0f);
    height = glm::mix(height, -7.50f, quarryMask);

    const float nileMask =
        roundedBoxMask(point, {0.0f, -166.0f}, {180.0f, 13.0f}, 7.0f);
    return glm::mix(height, -0.35f, nileMask);
}

float triangleInterpolate(float topLeft, float topRight, float bottomLeft,
                          float bottomRight, float localX, float localZ)
{
    if (localX + localZ <= 1.0f)
        return topLeft + (topRight - topLeft) * localX +
               (bottomLeft - topLeft) * localZ;
    return bottomRight + (bottomLeft - bottomRight) * (1.0f - localX) +
           (topRight - bottomRight) * (1.0f - localZ);
}
} // namespace

SandSimulation::SandSimulation()
{
    initGrid();
}

void SandSimulation::initGrid()
{
    grid_.assign(GridColumns * GridRows, SandCell{});
    initialVolume_ = 0.0f;

    for (int z = 0; z < GridRows; ++z)
    {
        for (int x = 0; x < GridColumns; ++x)
        {
            const glm::vec2 wPos = cellWorldPos(x, z);
            SandCell& cell = grid_[gridIndex(x, z)];

            // Obstacles: Pyramid footprint (center 0, -42, size ~105x105)
            const bool inPyramid = std::abs(wPos.x) < 54.0f && std::abs(wPos.y - (-42.0f)) < 54.0f;
            // Quarry hole (-128, -15, size 64x72)
            const bool inQuarry = std::abs(wPos.x - (-128.0f)) < 32.0f && std::abs(wPos.y - (-15.0f)) < 36.0f;
            // Sphinx area (92, -105, radius 18)
            const bool nearSphinx = glm::distance(wPos, glm::vec2{92.0f, -105.0f}) < 18.0f;

            cell.isObstacle = inPyramid || inQuarry || nearSphinx;

            // Haul traffic route: from quarry (-108, -5) to pyramid ramp (0, 45)
            const bool nearHaulLane = (wPos.y >= -10.0f && wPos.y <= 48.0f && wPos.x >= -115.0f && wPos.x <= 15.0f &&
                                       std::abs(wPos.y - (0.45f * wPos.x + 44.0f)) < 8.0f);
            cell.isTrafficRoute = nearHaulLane;

            // The base and sand depth sum to the exact deterministic terrain
            // surface. Future transport may alter the depth without requiring
            // a second rendering representation.
            cell.height = initialSandDepth;
            cell.baseElevation =
                baseSurfaceFormula(wPos.x, wPos.y) - initialSandDepth;

            cell.windExposure = inPyramid ? 0.35f : nearSphinx ? 0.55f : 1.0f;
            cell.state = SandCellState::Settled;
            initialVolume_ += cell.height;
        }
    }
}

void SandSimulation::reset()
{
    initGrid();
    simulationTimer_ = 0.0f;
}

glm::vec2 SandSimulation::cellWorldPos(int x, int z) const
{
    return {WorldMinX + static_cast<float>(x) * CellSizeX,
            WorldMinZ + static_cast<float>(z) * CellSizeZ};
}

bool SandSimulation::worldToGrid(float worldX, float worldZ, int& outX, int& outZ,
                                 float& localX, float& localZ) const
{
    const float fx = (worldX - WorldMinX) / CellSizeX;
    const float fz = (worldZ - WorldMinZ) / CellSizeZ;
    if (fx < 0.0f || fx > static_cast<float>(GridColumns - 1) ||
        fz < 0.0f || fz > static_cast<float>(GridRows - 1))
        return false;
    outX = std::min(static_cast<int>(std::floor(fx)), GridColumns - 2);
    outZ = std::min(static_cast<int>(std::floor(fz)), GridRows - 2);
    localX = std::clamp(fx - static_cast<float>(outX), 0.0f, 1.0f);
    localZ = std::clamp(fz - static_cast<float>(outZ), 0.0f, 1.0f);
    return true;
}

float SandSimulation::sampleSurface(float worldX, float worldZ,
                                    bool sandDepthOnly) const
{
    int gx = 0, gz = 0;
    float localX = 0.0f, localZ = 0.0f;
    if (!worldToGrid(worldX, worldZ, gx, gz, localX, localZ))
        return sandDepthOnly ? initialSandDepth
                             : staticTerrainHeightAt(worldX, worldZ);
    const auto value = [this, sandDepthOnly](int x, int z)
    {
        const SandCell& cell = grid_[gridIndex(x, z)];
        return sandDepthOnly ? cell.height : cell.baseElevation + cell.height;
    };
    return triangleInterpolate(value(gx, gz), value(gx + 1, gz),
                               value(gx, gz + 1), value(gx + 1, gz + 1),
                               localX, localZ);
}

float SandSimulation::sandHeightAt(float worldX, float worldZ) const
{
    return sampleSurface(worldX, worldZ, true);
}

float SandSimulation::terrainHeightAt(float worldX, float worldZ) const
{
    return sampleSurface(worldX, worldZ, false);
}

float SandSimulation::staticTerrainHeightAt(float worldX, float worldZ)
{
    const float fx = std::clamp((worldX - WorldMinX) / CellSizeX, 0.0f,
                                static_cast<float>(GridColumns - 1));
    const float fz = std::clamp((worldZ - WorldMinZ) / CellSizeZ, 0.0f,
                                static_cast<float>(GridRows - 1));
    const int x = std::min(static_cast<int>(std::floor(fx)), GridColumns - 2);
    const int z = std::min(static_cast<int>(std::floor(fz)), GridRows - 2);
    const float localX = std::clamp(fx - static_cast<float>(x), 0.0f, 1.0f);
    const float localZ = std::clamp(fz - static_cast<float>(z), 0.0f, 1.0f);
    const float x0 = WorldMinX + static_cast<float>(x) * CellSizeX;
    const float x1 = x0 + CellSizeX;
    const float z0 = WorldMinZ + static_cast<float>(z) * CellSizeZ;
    const float z1 = z0 + CellSizeZ;
    return triangleInterpolate(baseSurfaceFormula(x0, z0),
                               baseSurfaceFormula(x1, z0),
                               baseSurfaceFormula(x0, z1),
                               baseSurfaceFormula(x1, z1), localX, localZ);
}

glm::vec3 SandSimulation::staticTerrainNormalAt(float worldX, float worldZ)
{
    constexpr float offset = 0.5f;
    const float left = staticTerrainHeightAt(worldX - offset, worldZ);
    const float right = staticTerrainHeightAt(worldX + offset, worldZ);
    const float back = staticTerrainHeightAt(worldX, worldZ - offset);
    const float front = staticTerrainHeightAt(worldX, worldZ + offset);
    return glm::normalize(glm::vec3{left - right, 2.0f * offset, back - front});
}

SandCellState SandSimulation::cellStateAt(float worldX, float worldZ) const
{
    int gx = 0, gz = 0;
    float localX = 0.0f, localZ = 0.0f;
    if (!worldToGrid(worldX, worldZ, gx, gz, localX, localZ))
        return SandCellState::Settled;
    return grid_[gridIndex(gx, gz)].state;
}

void SandSimulation::setWind(const glm::vec2& dir, float speed)
{
    if (glm::length(dir) > 0.001f)
        windDirection_ = glm::normalize(dir);
    windSpeed_ = std::clamp(speed, 0.0f, 20.0f);
}

void SandSimulation::update(float deltaTime, const glm::vec3& sledgePos)
{
    if (!enabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    simulationTimer_ += deltaTime;

    // Fixed sub-stepping for numerical stability
    constexpr float fixedStep = 0.05f;
    float remaining = std::min(deltaTime, 0.25f);
    while (remaining > 0.0f)
    {
        const float step = std::min(remaining, fixedStep);
        simulateWindTransport(step);
        simulateReposeRelaxation(step);
        remaining -= step;
    }

    applyTrafficDisturbance(sledgePos);
}

void SandSimulation::simulateWindTransport(float deltaTime)
{
    const int stepX = (windDirection_.x > 0.1f) ? 1 : (windDirection_.x < -0.1f) ? -1 : 0;
    const int stepZ = (windDirection_.y > 0.1f) ? 1 : (windDirection_.y < -0.1f) ? -1 : 0;

    const float fluxRate = 0.015f * (windSpeed_ / 4.0f) * deltaTime;

    for (int z = 1; z < GridRows - 1; ++z)
    {
        for (int x = 1; x < GridColumns - 1; ++x)
        {
            SandCell& src = grid_[gridIndex(x, z)];
            if (src.height <= 0.05f || src.isObstacle)
                continue;

            const int nx = x + stepX;
            const int nz = z + stepZ;
            if (nx < 0 || nx >= GridColumns || nz < 0 || nz >= GridRows)
                continue;

            SandCell& dst = grid_[gridIndex(nx, nz)];

            // If destination is an obstacle, sand accumulates against its base
            float transfer = fluxRate * src.windExposure;
            transfer = std::min(transfer, src.height - 0.02f);

            if (transfer > 0.0f)
            {
                src.height -= transfer;
                dst.height += transfer;

                if (dst.isObstacle || dst.windExposure < 0.6f)
                    dst.state = SandCellState::Accumulating;
                else
                    src.state = SandCellState::WindMoved;
            }
        }
    }
}

void SandSimulation::simulateReposeRelaxation(float deltaTime)
{
    const float relaxRate = std::min(0.6f, 3.5f * deltaTime);

    for (int z = 1; z < GridRows - 1; ++z)
    {
        for (int x = 1; x < GridColumns - 1; ++x)
        {
            SandCell& center = grid_[gridIndex(x, z)];
            if (center.isObstacle)
                continue;

            const int neighbors[4][2] = {{x + 1, z}, {x - 1, z}, {x, z + 1}, {x, z - 1}};
            for (const auto& nb : neighbors)
            {
                SandCell& adj = grid_[gridIndex(nb[0], nb[1])];
                if (adj.isObstacle)
                    continue;

                const float totalH1 = center.baseElevation + center.height;
                const float totalH2 = adj.baseElevation + adj.height;
                const float diff = totalH1 - totalH2;

                if (diff > ReposeThreshold && center.height > 0.02f)
                {
                    const float transfer = std::min((diff - ReposeThreshold) * relaxRate,
                                                    center.height - 0.01f);
                    if (transfer > 0.0f)
                    {
                        center.height -= transfer;
                        adj.height += transfer;
                        center.state = SandCellState::Sliding;
                        adj.state = SandCellState::Settled;
                    }
                }
            }
        }
    }
}

void SandSimulation::applyTrafficDisturbance(const glm::vec3& sledgePos)
{
    int sx = 0, sz = 0;
    float localX = 0.0f, localZ = 0.0f;
    if (worldToGrid(sledgePos.x, sledgePos.z, sx, sz, localX, localZ))
    {
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = sx + dx;
                const int z = sz + dz;
                if (x >= 0 && x < GridColumns && z >= 0 && z < GridRows)
                {
                    SandCell& cell = grid_[gridIndex(x, z)];
                    // Compress and clear track rut
                    if (cell.height > 0.12f)
                    {
                        const float cleared = 0.035f;
                        cell.height -= cleared;
                        // Berm buildup on edges
                        const int edgeX = std::clamp(x + (dx != 0 ? dx : 1), 0, GridColumns - 1);
                        const int edgeZ = std::clamp(z + (dz != 0 ? dz : 1), 0, GridRows - 1);
                        grid_[gridIndex(edgeX, edgeZ)].height += cleared;
                    }
                }
            }
        }
    }
}

SandSimulationStats SandSimulation::stats() const
{
    SandSimulationStats s;
    s.minSandDepth = 999.0f;
    for (const auto& cell : grid_)
    {
        s.totalVolume += cell.height;
        s.maxAccumulation = std::max(s.maxAccumulation, cell.height);
        s.minSandDepth = std::min(s.minSandDepth, cell.height);
        if (cell.state == SandCellState::Sliding)
            ++s.slidingCells;
        else if (cell.state == SandCellState::Accumulating)
            ++s.accumulatingCells;
    }
    return s;
}

MeshData SandSimulation::generateTerrainMesh() const
{
    MeshData mesh("SandTerrain");
    if (!enabled_) return mesh;

    mesh.vertices.reserve(GridColumns * GridRows);
    for (int z = 0; z < GridRows; ++z)
    {
        for (int x = 0; x < GridColumns; ++x)
        {
            const SandCell& cell = grid_[gridIndex(x, z)];
            const glm::vec2 wPos = cellWorldPos(x, z);
            Vertex v;
            v.position = {wPos.x, cell.baseElevation + cell.height, wPos.y};
            v.normal = {0.0f, 1.0f, 0.0f};
            v.texCoord = {wPos.x * 0.035f, wPos.y * 0.035f};
            mesh.vertices.push_back(v);
        }
    }

    for (int z = 0; z < GridRows; ++z)
    {
        for (int x = 0; x < GridColumns; ++x)
        {
            glm::vec3 p = mesh.vertices[gridIndex(x, z)].position;
            glm::vec3 dx = (x < GridColumns - 1) ? mesh.vertices[gridIndex(x + 1, z)].position - p : p - mesh.vertices[gridIndex(x - 1, z)].position;
            glm::vec3 dz = (z < GridRows - 1) ? mesh.vertices[gridIndex(x, z + 1)].position - p : p - mesh.vertices[gridIndex(x, z - 1)].position;
            
            if (x > 0 && x < GridColumns - 1) dx = mesh.vertices[gridIndex(x + 1, z)].position - mesh.vertices[gridIndex(x - 1, z)].position;
            if (z > 0 && z < GridRows - 1) dz = mesh.vertices[gridIndex(x, z + 1)].position - mesh.vertices[gridIndex(x, z - 1)].position;

            glm::vec3 n = glm::normalize(glm::cross(dz, dx));
            if (n.y < 0.0f) n = -n;
            mesh.vertices[gridIndex(x, z)].normal = n;
        }
    }

    mesh.indices.reserve((GridColumns - 1) * (GridRows - 1) * 6);
    for (int z = 0; z < GridRows - 1; ++z)
    {
        for (int x = 0; x < GridColumns - 1; ++x)
        {
            std::uint32_t tl = gridIndex(x, z);
            std::uint32_t tr = gridIndex(x + 1, z);
            std::uint32_t bl = gridIndex(x, z + 1);
            std::uint32_t br = gridIndex(x + 1, z + 1);

            mesh.indices.push_back(tl);
            mesh.indices.push_back(bl);
            mesh.indices.push_back(tr);

            mesh.indices.push_back(tr);
            mesh.indices.push_back(bl);
            mesh.indices.push_back(br);
        }
    }

    return mesh;
}

bool SandSimulation::validateSandSimulation(std::ostream& output)
{
    SandSimulation sim;
    const SandSimulationStats initial = sim.stats();

    bool positiveHeights = true;
    for (int i = 0; i < 40; ++i)
    {
        sim.update(0.1f, glm::vec3{-50.0f, 0.0f, 20.0f});
        const SandSimulationStats current = sim.stats();
        positiveHeights = positiveHeights && (current.minSandDepth >= 0.0f);
    }

    const SandSimulationStats finalStats = sim.stats();
    // Mass conservation: total volume should match initial within numerical drift
    const float volumeDiff = std::abs(finalStats.totalVolume - initial.totalVolume);
    const bool conserved = volumeDiff < 0.05f;

    // Repose check: no dynamic sand-depth jump between open cells is excessive.
    bool reposeValid = true;
    for (int z = 1; z < SandSimulation::GridRows - 1; ++z)
    {
        for (int x = 1; x < SandSimulation::GridColumns - 1; ++x)
        {
            const auto& c1 = sim.cellAt(x, z);
            const auto& c2 = sim.cellAt(x + 1, z);
            if (c1.isObstacle || c2.isObstacle)
                continue; // stone walls/monuments act as physical retaining structures
            if (std::abs(c1.height - c2.height) > SandSimulation::ReposeThreshold * 2.0f)
                reposeValid = false;
        }
    }

    SandSimulation terrainSource;
    SandSimulation duplicateSource;
    const MeshData terrain = terrainSource.generateTerrainMesh();
    const MeshData duplicate = duplicateSource.generateTerrainMesh();
    bool verticesFinite = terrain.vertices.size() ==
                          static_cast<std::size_t>(GridColumns * GridRows);
    bool normalsValid = true;
    float maximumNeighborDifference = 0.0f;
    for (int z = 0; z < GridRows; ++z)
        for (int x = 0; x < GridColumns; ++x)
        {
            const Vertex& vertex = terrain.vertices[terrainSource.gridIndex(x, z)];
            verticesFinite = verticesFinite &&
                std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
                std::isfinite(vertex.position.z) && std::isfinite(vertex.normal.x) &&
                std::isfinite(vertex.normal.y) && std::isfinite(vertex.normal.z);
            normalsValid = normalsValid &&
                std::abs(glm::length(vertex.normal) - 1.0f) < 1.0e-3f;
            if (x + 1 < GridColumns)
                maximumNeighborDifference = std::max(
                    maximumNeighborDifference,
                    std::abs(vertex.position.y - terrain.vertices[
                        terrainSource.gridIndex(x + 1, z)].position.y));
            if (z + 1 < GridRows)
                maximumNeighborDifference = std::max(
                    maximumNeighborDifference,
                    std::abs(vertex.position.y - terrain.vertices[
                        terrainSource.gridIndex(x, z + 1)].position.y));
        }

    bool indicesValid = terrain.indices.size() ==
                        static_cast<std::size_t>((GridColumns - 1) *
                                                 (GridRows - 1) * 6);
    for (const std::uint32_t index : terrain.indices)
        indicesValid = indicesValid && index < terrain.vertices.size();
    const bool boundsValid =
        terrain.vertices.front().position.x == WorldMinX &&
        terrain.vertices.front().position.z == WorldMinZ &&
        terrain.vertices.back().position.x == WorldMaxX &&
        terrain.vertices.back().position.z == WorldMaxZ;
    const bool slopesBounded = maximumNeighborDifference < 2.0f;
    const bool workZonesLevel =
        std::abs(staticTerrainHeightAt(0.0f, -42.0f)) < 1.0e-4f &&
        std::abs(staticTerrainHeightAt(-55.0f, 25.0f)) < 1.0e-4f &&
        std::abs(staticTerrainHeightAt(-50.0f, 20.0f)) < 1.0e-4f &&
        std::abs(staticTerrainHeightAt(-128.0f, -15.0f) + 7.5f) < 1.0e-4f;
    bool queryMatchesMesh = true;
    for (int z = 0; z < GridRows; z += 10)
        for (int x = 0; x < GridColumns; x += 12)
        {
            const Vertex& vertex = terrain.vertices[terrainSource.gridIndex(x, z)];
            queryMatchesMesh = queryMatchesMesh &&
                std::abs(terrainSource.terrainHeightAt(vertex.position.x,
                                                       vertex.position.z) -
                         vertex.position.y) < 1.0e-5f &&
                std::abs(staticTerrainHeightAt(vertex.position.x,
                                               vertex.position.z) -
                         vertex.position.y) < 1.0e-5f;
        }
    bool deterministic = terrain.vertices.size() == duplicate.vertices.size() &&
                         terrain.indices == duplicate.indices;
    for (std::size_t index = 0; deterministic && index < terrain.vertices.size(); ++index)
        deterministic = glm::distance(terrain.vertices[index].position,
                                      duplicate.vertices[index].position) < 1.0e-7f;

    const bool valid = positiveHeights && conserved && reposeValid && verticesFinite &&
                       normalsValid && indicesValid && boundsValid && slopesBounded &&
                       workZonesLevel && queryMatchesMesh && deterministic;

    output << "Phase 13 Sand Simulation Validation\n"
           << "  all cell heights strictly non-negative: "
           << (positiveHeights ? "PASS" : "FAIL") << '\n'
           << "  total sand volume conserved across timesteps (delta " << volumeDiff << " < 0.05): "
           << (conserved ? "PASS" : "FAIL") << '\n'
           << "  angle-of-repose relaxation bounded slopes: "
           << (reposeValid ? "PASS" : "FAIL") << '\n'
           << "  continuous indexed terrain vertices/indices: "
           << (verticesFinite && indicesValid ? "PASS" : "FAIL") << '\n'
           << "  finite unit terrain normals: "
           << (normalsValid ? "PASS" : "FAIL") << '\n'
           << "  complete 420 x 330 bounds and bounded neighbor delta (max "
           << maximumNeighborDifference << "): "
           << (boundsValid && slopesBounded ? "PASS" : "FAIL") << '\n'
           << "  construction zones level and quarry floor preserved: "
           << (workZonesLevel ? "PASS" : "FAIL") << '\n'
           << "  rendered vertex heights match terrain query: "
           << (queryMatchesMesh ? "PASS" : "FAIL") << '\n'
           << "  deterministic terrain generation: "
           << (deterministic ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sand simulation validation passed.\n"
                     : "Sand simulation validation failed.\n");

    return valid;
}
