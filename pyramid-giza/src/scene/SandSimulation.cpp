#include "scene/SandSimulation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>



SandSimulation::SandSimulation()
{
    initGrid();
}

void SandSimulation::initGrid()
{
    grid_.assign(GridResolution * GridResolution, SandCell{});
    initialVolume_ = 0.0f;

    for (int z = 0; z < GridResolution; ++z)
    {
        for (int x = 0; x < GridResolution; ++x)
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

            // Base bedrock elevation
            cell.baseElevation = inQuarry ? -6.5f : 0.0f;

            // Initial deterministic sand depth
            if (inPyramid)
                cell.height = 0.15f; // stone foundation courses
            else if (inQuarry)
                cell.height = 0.15f;
            else if (cell.isTrafficRoute)
                cell.height = 0.20f; // swept haul path
            else
            {
                // Gentle natural dunes
                const float wave = std::sin(wPos.x * 0.035f) * std::cos(wPos.y * 0.030f);
                cell.height = 0.35f + 0.12f * wave;
            }

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
    const float half = GridExtent * 0.5f;
    return glm::vec2{-half + (static_cast<float>(x) + 0.5f) * CellSize,
                     -half + (static_cast<float>(z) + 0.5f) * CellSize};
}

bool SandSimulation::worldToGrid(float worldX, float worldZ, int& outX, int& outZ) const
{
    const float half = GridExtent * 0.5f;
    const float fx = (worldX + half) / CellSize;
    const float fz = (worldZ + half) / CellSize;
    if (fx < 0.0f || fx >= static_cast<float>(GridResolution) ||
        fz < 0.0f || fz >= static_cast<float>(GridResolution))
        return false;
    outX = static_cast<int>(fx);
    outZ = static_cast<int>(fz);
    return true;
}

float SandSimulation::sandHeightAt(float worldX, float worldZ) const
{
    int gx = 0, gz = 0;
    if (!worldToGrid(worldX, worldZ, gx, gz))
        return 0.35f;
    return grid_[gridIndex(gx, gz)].height;
}

SandCellState SandSimulation::cellStateAt(float worldX, float worldZ) const
{
    int gx = 0, gz = 0;
    if (!worldToGrid(worldX, worldZ, gx, gz))
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

    for (int z = 1; z < GridResolution - 1; ++z)
    {
        for (int x = 1; x < GridResolution - 1; ++x)
        {
            SandCell& src = grid_[gridIndex(x, z)];
            if (src.height <= 0.05f || src.isObstacle)
                continue;

            const int nx = x + stepX;
            const int nz = z + stepZ;
            if (nx < 0 || nx >= GridResolution || nz < 0 || nz >= GridResolution)
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

    for (int z = 1; z < GridResolution - 1; ++z)
    {
        for (int x = 1; x < GridResolution - 1; ++x)
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
    if (worldToGrid(sledgePos.x, sledgePos.z, sx, sz))
    {
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = sx + dx;
                const int z = sz + dz;
                if (x >= 0 && x < GridResolution && z >= 0 && z < GridResolution)
                {
                    SandCell& cell = grid_[gridIndex(x, z)];
                    // Compress and clear track rut
                    if (cell.height > 0.12f)
                    {
                        const float cleared = 0.035f;
                        cell.height -= cleared;
                        // Berm buildup on edges
                        const int edgeX = std::clamp(x + (dx != 0 ? dx : 1), 0, GridResolution - 1);
                        const int edgeZ = std::clamp(z + (dz != 0 ? dz : 1), 0, GridResolution - 1);
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

void SandSimulation::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    // Generate instanced/shared sand drift patches at accumulating cells and dune crests
    for (int z = 1; z < GridResolution - 1; z += 2)
    {
        for (int x = 1; x < GridResolution - 1; x += 2)
        {
            const SandCell& cell = grid_[gridIndex(x, z)];
            if (cell.height < 0.40f)
                continue;

            const glm::vec2 wPos = cellWorldPos(x, z);
            const float h = cell.height;
            const glm::vec3 pos{wPos.x, cell.baseElevation + h * 0.5f, wPos.y};
            const glm::vec3 scale{CellSize * 1.85f, h, CellSize * 1.85f};

            objects.push_back({ScenePrimitive::Cube,
                               makeTransform(pos, {0.0f, static_cast<float>((x * 37 + z * 19) % 360), 0.0f}, scale),
                               MaterialId::Sand});
        }
    }
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

    // Repose check: no slope between open terrain cells should severely exceed ReposeThreshold
    bool reposeValid = true;
    for (int z = 1; z < SandSimulation::GridResolution - 1; ++z)
    {
        for (int x = 1; x < SandSimulation::GridResolution - 1; ++x)
        {
            const auto& c1 = sim.cellAt(x, z);
            const auto& c2 = sim.cellAt(x + 1, z);
            if (c1.isObstacle || c2.isObstacle)
                continue; // stone walls/monuments act as physical retaining structures
            if (std::abs(c1.height - c2.height) > SandSimulation::ReposeThreshold * 2.0f)
                reposeValid = false;
        }
    }

    const bool valid = positiveHeights && conserved && reposeValid;

    output << "Phase 13 Sand Simulation Validation\n"
           << "  all cell heights strictly non-negative: "
           << (positiveHeights ? "PASS" : "FAIL") << '\n'
           << "  total sand volume conserved across timesteps (delta " << volumeDiff << " < 0.05): "
           << (conserved ? "PASS" : "FAIL") << '\n'
           << "  angle-of-repose relaxation bounded slopes: "
           << (reposeValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sand simulation validation passed.\n"
                     : "Sand simulation validation failed.\n");

    return valid;
}
