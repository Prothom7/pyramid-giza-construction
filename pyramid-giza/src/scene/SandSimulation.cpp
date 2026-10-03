#include "scene/SandSimulation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>
#include "scene/MonumentalSite.h"
#include "scene/SupportSystem.h"
#include "scene/IndustrialLandscape.h"



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

// Reuse the static grading masks and explicit support footprints. Haul-road
// shoulders stay mobile; foundations and structural surfaces never receive flux.
float mobilityAt(const glm::vec2& point)
{
    float protection = 0.0f;
    for (const SiteZoneDescriptor& zone : MonumentalSite::zones())
    {
        if (std::string(zone.id) == "TransportZone")
            continue;
        protection = std::max(protection,
            roundedBoxMask(point, {zone.center.x, zone.center.z},
                           {zone.extents.x + 4.0f, zone.extents.z + 4.0f}, 10.0f));
    }
    protection = std::max(protection,
        roundedBoxMask(point, {0.0f, -42.0f}, {68.0f, 70.0f}, 20.0f));
    protection = std::max(protection,
        roundedBoxMask(point, {-128.0f, -15.0f}, {35.0f, 37.0f}, 10.0f));
    protection = std::max(protection,
        roundedBoxMask(point, {0.0f, -157.0f}, {185.0f, 32.0f}, 10.0f));
    for (const RampDescriptor& ramp : MonumentalSite::ramps())
        protection = std::max(protection,
            capsuleMask(point, {ramp.base.x, ramp.base.z}, {ramp.top.x, ramp.top.z},
                        0.5f * ramp.width + 4.0f, 6.0f));
    // These southern depot shoulders and the north pit exit are graded desert,
    // not foundations. Keep the original masks everywhere else. Within this
    // open corridor, only actual structures and stored stones immobilize sand.
    if (point.x >= -135.0f && point.x <= 5.0f && point.y > 28.0f && point.y <= 64.0f)
    {
        protection = 0.0f;
        for (const RepositoryDescriptor& repository : IndustrialLandscape::repositories())
        {
            const glm::vec2 half{
                0.5f * (repository.columns * repository.blockScale.x + (repository.columns - 1) * repository.spacing),
                0.5f * (repository.rows * repository.blockScale.z + (repository.rows - 1) * repository.spacing)};
            protection = std::max(protection, roundedBoxMask(point,
                {repository.center.x, repository.center.z}, half + glm::vec2{2.0f}, 2.0f));
        }
        protection = std::max(protection, roundedBoxMask(point, {-10.0f, 42.0f}, {9.0f, 6.5f}, 2.0f));
        protection = std::max(protection, roundedBoxMask(point, {-30.0f, 51.0f}, {6.0f, 4.9f}, 2.0f));
        protection = std::max(protection, roundedBoxMask(point, {-13.5f, 48.5f}, {7.8f, 3.2f}, 2.0f));
        for (const RampDescriptor& ramp : MonumentalSite::ramps())
        {
            protection = std::max(protection, capsuleMask(point,
                {ramp.base.x, ramp.base.z}, {ramp.top.x, ramp.top.z}, 0.5f * ramp.width + 2.0f, 2.0f));
            if (std::string(ramp.id).find("MainHauling") == 0)
            {
                const RampDescriptor toe = SceneSupport::transportRampToe(ramp);
                protection = std::max(protection, capsuleMask(point,
                    {toe.base.x, toe.base.z}, {toe.top.x, toe.top.z}, 0.5f * toe.width + 2.0f, 2.0f));
            }
        }
    }
    return 1.0f - protection;
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
    flux_.assign(grid_.size(), 0.0f);
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

            cell.mobility = mobilityAt(wPos);
            cell.windExposure = cell.mobility;
            cell.state = SandCellState::Settled;
            initialVolume_ += cell.height;
        }
    }
}

void SandSimulation::reset()
{
    initGrid();
    simulationTimer_ = 0.0;
    previousContact_ = false;
    enabled_ = true;
    trackDepth_ = 0.0f;
    windDirection_ = glm::normalize(glm::vec2{-1.0f, 1.0f});
    windSpeed_ = 4.2f;
    ++revision_;
}

glm::vec2 SandSimulation::cellWorldPos(int x, int z) const
{
    return {WorldMinX + static_cast<float>(x) * CellSizeX,
            WorldMinZ + static_cast<float>(z) * CellSizeZ};
}

bool SandSimulation::worldToGrid(float worldX, float worldZ, int& outX, int& outZ,
                                 float& localX, float& localZ) const
{
    if (!std::isfinite(worldX) || !std::isfinite(worldZ))
        return false;
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
    if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(speed))
        return;
    if (glm::length(dir) > 0.001f)
        windDirection_ = glm::normalize(dir);
    windSpeed_ = std::clamp(speed, 0.0f, 20.0f);
}

void SandSimulation::update(float deltaTime, const glm::vec3& sledgePos,
                            float headingDegrees, bool exposedSandContact)
{
    exposedSandContact = exposedSandContact && std::isfinite(sledgePos.x) &&
        std::isfinite(sledgePos.y) && std::isfinite(sledgePos.z) &&
        std::isfinite(headingDegrees);
    if (!enabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
    {
        previousContact_ = false;
        return;
    }
    simulationTimer_ += static_cast<double>(deltaTime);
    bool changed = false;
    while (simulationTimer_ + 1.0e-8 >= FixedStep)
    {
        simulateWindTransport(static_cast<float>(FixedStep));
        simulateReposeRelaxation(static_cast<float>(FixedStep));
        simulationTimer_ -= FixedStep;
        changed = true;
    }
    // Traffic is distance-based rather than frame-based. Sampling the swept
    // segment prevents gaps at low FPS, and stationary sledges make no ruts.
    if (exposedSandContact && previousContact_ &&
        glm::distance(sledgePos, previousSledge_) < 10.0f)
    {
        applyTrafficDisturbance(previousSledge_, sledgePos, headingDegrees);
        changed = changed || glm::distance(sledgePos, previousSledge_) > 1.0e-5f;
    }
    previousSledge_ = sledgePos;
    previousContact_ = exposedSandContact;
    if (changed)
        ++revision_;
}

void SandSimulation::transferSand(std::size_t from, std::size_t to, float quantity)
{
    const float mobility = std::min(grid_[from].mobility, grid_[to].mobility);
    // Each pair shares exactly the same bounded debit and credit. Protected
    // vertices are impermeable; no clipping/renormalisation loses sand mass.
    const float available = std::max(0.0f, grid_[from].height + flux_[from]);
    const float capacity = std::max(0.0f,
        MaximumSandDepth - grid_[to].height - flux_[to]);
    const float transfer = std::min({quantity * mobility, available, capacity});
    flux_[from] -= transfer;
    flux_[to] += transfer;
}

void SandSimulation::simulateWindTransport(float deltaTime)
{
    std::fill(flux_.begin(), flux_.end(), 0.0f);
    const int stepX = windDirection_.x > 0.1f ? 1 : windDirection_.x < -0.1f ? -1 : 0;
    const int stepZ = windDirection_.y > 0.1f ? 1 : windDirection_.y < -0.1f ? -1 : 0;
    if (stepX == 0 && stepZ == 0)
        return;
    for (int z = 1; z < GridRows - 1; ++z)
        for (int x = 1; x < GridColumns - 1; ++x)
        {
            const std::size_t from = gridIndex(x, z);
            const std::size_t to = gridIndex(x + stepX, z + stepZ);
            const glm::vec2 point = cellWorldPos(x, z);
            // Gentle deterministic exposure variation forms broad deposits,
            // instead of draining the entire desert to a boundary ridge.
            const float exposure = 0.65f + 0.35f *
                std::sin(point.x * 0.035f + point.y * 0.024f);
            transferSand(from, to, 0.0025f * windSpeed_ * deltaTime *
                                   exposure * grid_[from].height / initialSandDepth);
        }
    for (std::size_t i = 0; i < grid_.size(); ++i)
    {
        grid_[i].height += flux_[i];
        if (flux_[i] != 0.0f)
            grid_[i].state = flux_[i] > 0.0f ? SandCellState::Accumulating
                                            : SandCellState::WindMoved;
    }
}

void SandSimulation::simulateReposeRelaxation(float deltaTime)
{
    std::fill(flux_.begin(), flux_.end(), 0.0f);
    for (int z = 0; z < GridRows; ++z)
        for (int x = 0; x < GridColumns; ++x)
            for (int axis = 0; axis < 2; ++axis)
            {
                const int nx = x + (axis == 0 ? 1 : 0);
                const int nz = z + (axis == 1 ? 1 : 0);
                if (nx >= GridColumns || nz >= GridRows)
                    continue;
                const std::size_t a = gridIndex(x, z), b = gridIndex(nx, nz);
                // Relax the movable offset, never the immutable quarry grade.
                const float difference = grid_[a].height - grid_[b].height;
                const float spacing = axis == 0 ? CellSizeX : CellSizeZ;
                const float allowed = 0.025f * spacing;
                if (std::abs(difference) > allowed)
                    transferSand(difference > 0.0f ? a : b,
                                 difference > 0.0f ? b : a,
                                 (std::abs(difference) - allowed) *
                                     std::min(0.25f, deltaTime * 2.0f));
            }
    for (std::size_t i = 0; i < grid_.size(); ++i)
        grid_[i].height += flux_[i];
}

void SandSimulation::applyTrafficDisturbance(const glm::vec3& start,
                                            const glm::vec3& end,
                                            float headingDegrees)
{
    const float distance = glm::length(glm::vec2{end.x - start.x, end.z - start.z});
    if (distance < 1.0e-5f)
        return;
    const glm::vec2 right{std::cos(glm::radians(headingDegrees)),
                          -std::sin(glm::radians(headingDegrees))};
    // The 3.3m vertex spacing cannot resolve 0.62m runner separation. Use one
    // smoothly reconstructed contact band with the actual 1.24m runner width,
    // broadened only by the grid reconstruction footprint (not a second mesh).
    const int samples = std::max(1, static_cast<int>(std::ceil(distance / 0.25f)));
    for (int sample = 0; sample < samples; ++sample)
    {
        const glm::vec3 center = glm::mix(start, end,
            (static_cast<float>(sample) + 0.5f) / static_cast<float>(samples));
        int gx = 0, gz = 0;
        float localX = 0.0f, localZ = 0.0f;
        if (!worldToGrid(center.x, center.z, gx, gz, localX, localZ))
            continue;
        for (int z = std::max(0, gz - 2); z <= std::min(GridRows - 1, gz + 2); ++z)
            for (int x = std::max(0, gx - 2); x <= std::min(GridColumns - 1, gx + 2); ++x)
            {
                const std::size_t from = gridIndex(x, z);
                const glm::vec2 offset = cellWorldPos(x, z) - glm::vec2{center.x, center.z};
                const float lateral = std::abs(glm::dot(offset, right));
                const float radial = glm::length(offset);
                const float weight = std::exp(-0.5f * lateral * lateral / 2.25f) *
                                     std::exp(-0.5f * radial * radial / 6.25f);
                if (weight < 0.04f || grid_[from].mobility < 0.99f)
                    continue;
                // Move the shallow rut into a nearby shoulder, with a 4cm cap.
                const int side = glm::dot(offset, right) < 0.0f ? -1 : 1;
                const int tx = std::clamp(x + side * static_cast<int>(
                    std::round(right.x * 2.0f)), 0, GridColumns - 1);
                const int tz = std::clamp(z + side * static_cast<int>(
                    std::round(right.y * 2.0f)), 0, GridRows - 1);
                const std::size_t to = gridIndex(tx, tz);
                if (to == from || grid_[to].mobility < 0.99f)
                    continue;
                const float amount = std::min({0.012f * distance /
                    static_cast<float>(samples) * weight,
                    std::max(0.0f, grid_[from].height - (initialSandDepth - 0.04f)),
                    std::max(0.0f, MaximumSandDepth - grid_[to].height)});
                grid_[from].height -= amount;
                grid_[to].height += amount;
                trackDepth_ = std::max(trackDepth_,
                    initialSandDepth - grid_[from].height);
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
        s.movableVolume += static_cast<double>(cell.height) * CellSizeX * CellSizeZ;
        const float change = std::abs(cell.height - initialSandDepth);
        s.maximumChange = std::max(s.maximumChange, change);
        if (change > 1.0e-5f) ++s.changedCells;
        if (cell.mobility == 0.0f)
            s.protectedChange = std::max(s.protectedChange, change);
        s.maxAccumulation = std::max(s.maxAccumulation, cell.height);
        s.minSandDepth = std::min(s.minSandDepth, cell.height);
        if (cell.state == SandCellState::Sliding)
            ++s.slidingCells;
        else if (cell.state == SandCellState::Accumulating)
            ++s.accumulatingCells;
    }
    s.trackDepth = trackDepth_;
    for (int z = 0; z < GridRows - 1; ++z)
        for (int x = 0; x < GridColumns - 1; ++x)
        {
            const float depth = grid_[gridIndex(x, z)].height;
            s.maximumSlope = std::max(s.maximumSlope,
                std::abs(depth - grid_[gridIndex(x + 1, z)].height) / CellSizeX);
            s.maximumSlope = std::max(s.maximumSlope,
                std::abs(depth - grid_[gridIndex(x, z + 1)].height) / CellSizeZ);
        }
    return s;
}

glm::vec3 SandSimulation::terrainNormalAt(float x, float z) const
{
    constexpr float offset = 0.5f;
    return glm::normalize(glm::vec3{
        terrainHeightAt(x - offset, z) - terrainHeightAt(x + offset, z),
        2.0f * offset,
        terrainHeightAt(x, z - offset) - terrainHeightAt(x, z + offset)});
}

MeshData SandSimulation::generateTerrainMesh() const
{
    MeshData mesh("SandTerrain");
    // Pause freezes transport, never removes the terrain surface.

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

    SandSimulation sixtyFps, thirtyFps;
    for (int frame = 0; frame < 1200; ++frame)
        sixtyFps.update(1.0f / 60.0f);
    for (int frame = 0; frame < 600; ++frame)
        thirtyFps.update(1.0f / 30.0f);
    float frameRateError = 0.0f;
    double downwindMoment = 0.0;
    bool dynamicFinite = true;
    for (std::size_t i = 0; i < sixtyFps.grid_.size(); ++i)
    {
        const SandCell& cell = sixtyFps.grid_[i];
        frameRateError = std::max(frameRateError,
            std::abs(cell.height - thirtyFps.grid_[i].height));
        const glm::vec2 point = sixtyFps.cellWorldPos(
            static_cast<int>(i) % GridColumns, static_cast<int>(i) / GridColumns);
        downwindMoment += (cell.height - initialSandDepth) *
                           glm::dot(point, sixtyFps.windDirection_);
        dynamicFinite = dynamicFinite && std::isfinite(cell.height) &&
                        cell.height >= -1.0e-6f &&
                        cell.height <= MaximumSandDepth + 1.0e-6f &&
                        cell.baseElevation == terrainSource.grid_[i].baseElevation;
    }
    const SandSimulationStats dynamicStats = sixtyFps.stats();
    const double relativeVolumeError = std::abs(dynamicStats.movableVolume -
        terrainSource.stats().movableVolume) / terrainSource.stats().movableVolume;
    const bool dynamicStable = dynamicFinite && relativeVolumeError < 1.0e-5 &&
        dynamicStats.protectedChange == 0.0f && dynamicStats.maximumSlope < 0.03f &&
        downwindMoment > 0.0 && frameRateError < 1.0e-6f;

    SandSimulation tracks;
    tracks.setWind({-1.0f, 1.0f}, 0.0f);
    tracks.update(0.05f, {170.0f, 0.0f, 50.0f}, 0.0f, true);
    for (int step = 1; step <= 100; ++step)
        tracks.update(0.05f, {170.0f, 0.0f, 50.0f + step * 0.20f}, 0.0f, true);
    const float depthBeforeRest = tracks.stats().trackDepth;
    const float rutHeight = tracks.terrainHeightAt(170.0f, 60.0f);
    const float offPathHeight = tracks.terrainHeightAt(150.0f, 60.0f);
    for (int step = 0; step < 100; ++step) tracks.update(0.05f);
    const bool tracksValid = depthBeforeRest > 0.001f && depthBeforeRest <= 0.04001f &&
        rutHeight < staticTerrainHeightAt(170.0f, 60.0f) - 0.001f &&
        std::abs(offPathHeight - staticTerrainHeightAt(150.0f, 60.0f)) < 1.0e-5f &&
        tracks.terrainHeightAt(170.0f, 60.0f) <
            staticTerrainHeightAt(170.0f, 60.0f) - 0.001f &&
        std::abs(tracks.stats().movableVolume - terrainSource.stats().movableVolume) /
            terrainSource.stats().movableVolume < 1.0e-5;
    SandSimulation platform;
    platform.setWind({1.0f, 0.0f}, 0.0f);
    platform.update(0.05f, {170.0f, 4.0f, 50.0f}, 0.0f, false);
    platform.update(0.05f, {170.0f, 4.0f, 55.0f}, 0.0f, false);
    const bool platformValid = platform.stats().trackDepth == 0.0f;
    const MeshData evolved = sixtyFps.generateTerrainMesh();
    bool liveQueryMatches = true;
    SceneSupport::setTerrainSource(&sixtyFps);
    for (std::size_t i = 0; i + 3 < evolved.indices.size(); i += 306)
    {
        const glm::vec3 a = evolved.vertices[evolved.indices[i]].position;
        const glm::vec3 b = evolved.vertices[evolved.indices[i + 1]].position;
        const glm::vec3 c = evolved.vertices[evolved.indices[i + 2]].position;
        const glm::vec3 center = (a + b + c) / 3.0f;
        liveQueryMatches = liveQueryMatches && std::abs(
            sixtyFps.terrainHeightAt(center.x, center.z) - center.y) < 1.0e-4f &&
            std::abs(SceneSupport::terrainAt({center.x, center.z}).height - center.y)
                < 1.0e-4f;
    }
    SceneSupport::setTerrainSource(nullptr);
    for (const Vertex& vertex : evolved.vertices)
        liveQueryMatches = liveQueryMatches && std::isfinite(vertex.normal.x) &&
            std::isfinite(vertex.normal.y) && std::isfinite(vertex.normal.z) &&
            std::abs(glm::length(vertex.normal) - 1.0f) < 1.0e-3f;
    sixtyFps.reset();
    bool resetValid = sixtyFps.trackDepth_ == 0.0f && sixtyFps.simulationTimer_ == 0.0 &&
                      sixtyFps.windSpeed_ == 4.2f;
    for (std::size_t i = 0; i < sixtyFps.grid_.size(); ++i)
        resetValid = resetValid && sixtyFps.grid_[i].height == terrainSource.grid_[i].height &&
                     sixtyFps.grid_[i].baseElevation == terrainSource.grid_[i].baseElevation;

    const bool valid = positiveHeights && conserved && reposeValid && verticesFinite &&
                       normalsValid && indicesValid && boundsValid && slopesBounded &&
                       workZonesLevel && queryMatchesMesh && deterministic &&
                       dynamicStable && tracksValid && platformValid && liveQueryMatches && resetValid;

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
           << "  dynamic volume initial/final: " << terrainSource.stats().movableVolume
           << " / " << dynamicStats.movableVolume << "; relative error "
           << relativeVolumeError << '\n'
           << "  maximum change/slope/protected change: " << dynamicStats.maximumChange
           << " / " << dynamicStats.maximumSlope << " / " << dynamicStats.protectedChange
           << "; changed cells " << dynamicStats.changedCells << '\n'
           << "  fixed-step wind/mass/protection/FPS checks (error " << frameRateError
           << "): " << (dynamicStable ? "PASS" : "FAIL") << '\n'
           << "  distance-based persistent track depth " << depthBeforeRest << ": "
           << (tracksValid ? "PASS" : "FAIL") << '\n'
           << "  no platform disturbance / live query / reset: "
           << (platformValid && liveQueryMatches && resetValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sand simulation validation passed.\n"
                     : "Sand simulation validation failed.\n");

    return valid;
}
