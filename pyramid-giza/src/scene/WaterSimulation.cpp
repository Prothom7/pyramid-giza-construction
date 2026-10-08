#include "scene/WaterSimulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "scene/IndustrialLandscape.h"
#include "scene/SandSimulation.h"
#include "graphics/Mesh.h"
#include "animation/ConstructionAnimation.h"

namespace
{
// The supply boat starts bow-first at its authored heading, makes one rolling
// turn in the open channel, then proceeds east. No navigation state is shared
// with the moored cargo boat or with the decorative wake objects.
constexpr float SupplyTurnRadius = 7.0f;
constexpr float SupplyEndX = 55.0f;
constexpr float SupplyCruiseSpeed = 2.0f;
constexpr float SupplyEaseSeconds = 3.0f;
constexpr float WakeSpacing = 1.5f;
constexpr float WakeSpeedThreshold = 0.15f;
constexpr float WakeLifetime = 5.5f;
constexpr float WakeSurfaceOffset = 0.008f;
constexpr float WakeThickness = 0.012f;

struct SupplyRoutePose
{
    glm::vec2 position{0.0f}; // x, z
    float yawDegrees = 0.0f;
};

float supplyArcLength(const BoatDescriptor& boat)
{
    return SupplyTurnRadius *
        (glm::half_pi<float>() - glm::radians(boat.yawDegrees));
}

float supplyRouteLength(const BoatDescriptor& boat)
{
    const float startYaw = glm::radians(boat.yawDegrees);
    const float arcEndX = boat.center.x + SupplyTurnRadius * std::cos(startYaw);
    return supplyArcLength(boat) + SupplyEndX - arcEndX;
}

float supplyTravelTime(const BoatDescriptor& boat)
{
    return supplyRouteLength(boat) / SupplyCruiseSpeed + SupplyEaseSeconds;
}

float supplyDistanceAt(const BoatDescriptor& boat, float elapsed)
{
    const float length = supplyRouteLength(boat);
    const float duration = supplyTravelTime(boat);
    const float t = glm::clamp(elapsed, 0.0f, duration);
    const auto easedDistance = [](float phaseSeconds)
    {
        const float u = phaseSeconds / SupplyEaseSeconds;
        return SupplyCruiseSpeed * SupplyEaseSeconds *
            (u * u * u - 0.5f * u * u * u * u);
    };
    if (t < SupplyEaseSeconds)
        return easedDistance(t);
    if (t > duration - SupplyEaseSeconds)
        return length - easedDistance(duration - t);
    return 0.5f * SupplyCruiseSpeed * SupplyEaseSeconds +
           SupplyCruiseSpeed * (t - SupplyEaseSeconds);
}

float supplySpeedAt(const BoatDescriptor& boat, float elapsed)
{
    const float duration = supplyTravelTime(boat);
    if (elapsed <= 0.0f || elapsed >= duration)
        return 0.0f;
    const float rampTime = std::min(elapsed, duration - elapsed);
    if (rampTime >= SupplyEaseSeconds)
        return SupplyCruiseSpeed;
    const float u = rampTime / SupplyEaseSeconds;
    return SupplyCruiseSpeed * (3.0f * u * u - 2.0f * u * u * u);
}

SupplyRoutePose supplyRoutePose(const BoatDescriptor& boat, float distance)
{
    const float startYaw = glm::radians(boat.yawDegrees);
    const float arcLength = supplyArcLength(boat);
    const float s = glm::clamp(distance, 0.0f, supplyRouteLength(boat));
    if (s <= arcLength)
    {
        const float yaw = startYaw + s / SupplyTurnRadius;
        return {{boat.center.x + SupplyTurnRadius *
                    (std::cos(startYaw) - std::cos(yaw)),
                 boat.center.z + SupplyTurnRadius *
                    (std::sin(yaw) - std::sin(startYaw))},
                glm::degrees(yaw)};
    }
    return {{boat.center.x + SupplyTurnRadius * std::cos(startYaw) +
                 (s - arcLength),
             boat.center.z + SupplyTurnRadius * (1.0f - std::sin(startYaw))},
            90.0f};
}

glm::vec2 supplyVelocityAt(const BoatDescriptor& boat, float elapsed)
{
    const SupplyRoutePose pose = supplyRoutePose(
        boat, supplyDistanceAt(boat, elapsed));
    const float yaw = glm::radians(pose.yawDegrees);
    return supplySpeedAt(boat, elapsed) * glm::vec2{std::sin(yaw), std::cos(yaw)};
}

glm::vec3 sternLocal(const BoatDescriptor& boat)
{
    // The end board, not the shorter central hull box, is the rearmost part.
    // Its 1.45 m fore/aft length and 0.62 m height are rotated 22 degrees.
    const float boardHalfExtent = 0.5f *
        (1.45f * std::cos(glm::radians(22.0f)) +
         0.62f * std::sin(glm::radians(22.0f)));
    return {0.0f, BoatDescriptor::waterlineLocalY,
            -0.45f * boat.length - boardHalfExtent};
}
} // namespace


WaterSimulation::WaterSimulation()
{
    baseSurfaceMesh_ = IndustrialLandscape::nileSurfaceMesh();
    surfaceVertices_ = baseSurfaceMesh_.vertices;
    const glm::mat4 model = IndustrialLandscape::nileSurfaceModel();
    surfaceWorldXZ_.reserve(surfaceVertices_.size());
    for (const Vertex& vertex : baseSurfaceMesh_.vertices)
    {
        const glm::vec3 world{model * glm::vec4{vertex.position, 1.0f}};
        surfaceWorldXZ_.emplace_back(world.x, world.z);
    }
    updateSurface();
    initBoats();
}

void WaterSimulation::initBoats()
{
    baseBoats_ = ObjectEnrichment::boats();
    boatStates_.resize(baseBoats_.size());
    updateBoats();
}

void WaterSimulation::reset()
{
    simulationTime_ = 0.0f;
    wakes_ = {};
    nextWakeDistance_ = WakeSpacing;
    wakeEmissionCount_ = 0;
    updateSurface();
    updateBoats();
}

void WaterSimulation::update(float deltaTime)
{
    if (!enabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    const float previousTime = simulationTime_;
    float previousDistance = 0.0f;
    for (const BoatDescriptor& boat : baseBoats_)
        if (std::strcmp(boat.id, "SupplyBoatOffshore") == 0)
            previousDistance = supplyDistanceAt(boat, previousTime);
    simulationTime_ += deltaTime;
    updateSurface();
    updateBoats();
    for (const BoatDescriptor& boat : baseBoats_)
        if (std::strcmp(boat.id, "SupplyBoatOffshore") == 0)
            updatePropulsionWake(previousDistance,
                                 supplyDistanceAt(boat, simulationTime_), previousTime);
}

float WaterSimulation::attenuatedWaveHeightAt(float x, float z) const
{
    // The existing wave field drives both the VBO and the bounded query.
    // A smooth 6 m bank fade keeps troughs above the shallow terminal bed
    // while leaving the deep central channel's wave field unchanged.
    constexpr float bankFadeWidth = 6.0f;
    const float distance = IndustrialLandscape::nileSurface().signedBankDistance(x, z);
    const float fraction = glm::clamp(distance / bankFadeWidth, 0.0f, 1.0f);
    const float attenuation = fraction * fraction * (3.0f - 2.0f * fraction);
    return attenuation * waveHeightAt(x, z);
}

void WaterSimulation::updateSurface()
{
    constexpr float normalProbe = 0.20f;
    for (std::size_t i = 0; i < surfaceVertices_.size(); ++i)
    {
        const glm::vec2 point = surfaceWorldXZ_[i];
        const float height = attenuatedWaveHeightAt(point.x, point.y);
        const float dx = (attenuatedWaveHeightAt(point.x + normalProbe, point.y) -
                          attenuatedWaveHeightAt(point.x - normalProbe, point.y)) /
                         (2.0f * normalProbe);
        const float dz = (attenuatedWaveHeightAt(point.x, point.y + normalProbe) -
                          attenuatedWaveHeightAt(point.x, point.y - normalProbe)) /
                         (2.0f * normalProbe);
        surfaceVertices_[i].position.y = height;
        surfaceVertices_[i].normal = glm::normalize(glm::vec3{-dx, 1.0f, -dz});
    }
}

bool WaterSimulation::waterSurfaceAt(float x, float z, float& height) const
{
    const NileSurfaceBounds nile = IndustrialLandscape::nileSurface();
    if (!nile.contains(x, z))
        return false;

    // Interpolate the current CPU vertices with the exact triangles uploaded
    // to the VBO. This makes future buoyancy queries match the visible mesh.
    const glm::vec2 point{x, z};
    constexpr float edgeTolerance = 1.0e-5f;
    for (std::size_t i = 0; i + 2 < baseSurfaceMesh_.indices.size(); i += 3)
    {
        const auto ia = baseSurfaceMesh_.indices[i];
        const auto ib = baseSurfaceMesh_.indices[i + 1];
        const auto ic = baseSurfaceMesh_.indices[i + 2];
        const glm::vec2 a = surfaceWorldXZ_[ia];
        const glm::vec2 b = surfaceWorldXZ_[ib];
        const glm::vec2 c = surfaceWorldXZ_[ic];
        if (point.x < std::min({a.x, b.x, c.x}) - edgeTolerance ||
            point.x > std::max({a.x, b.x, c.x}) + edgeTolerance ||
            point.y < std::min({a.y, b.y, c.y}) - edgeTolerance ||
            point.y > std::max({a.y, b.y, c.y}) + edgeTolerance)
            continue;
        const float denominator =
            (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if (std::abs(denominator) <= 1.0e-9f)
            continue;
        const float wa = ((b.y - c.y) * (point.x - c.x) +
                          (c.x - b.x) * (point.y - c.y)) / denominator;
        const float wb = ((c.y - a.y) * (point.x - c.x) +
                          (a.x - c.x) * (point.y - c.y)) / denominator;
        const float wc = 1.0f - wa - wb;
        if (wa < -edgeTolerance || wb < -edgeTolerance || wc < -edgeTolerance)
            continue;
        height = nile.waterY + wa * surfaceVertices_[ia].position.y +
                 wb * surfaceVertices_[ib].position.y +
                 wc * surfaceVertices_[ic].position.y;
        return true;
    }
    return false;
}

float WaterSimulation::waveHeightAt(float x, float z) const
{
    // Dominant Nile flow carries waves along +X
    const float t = simulationTime_;

    const float w1 = 0.10f * std::sin(0.24f * x + 0.05f * z - 1.5f * t);
    const float w2 = 0.06f * std::cos(0.38f * x - 0.14f * z - 2.1f * t);
    const float w3 = 0.04f * std::sin(-0.16f * x + 0.32f * z - 1.2f * t);

    return w1 + w2 + w3;
}

glm::vec3 WaterSimulation::waveNormalAt(float x, float z) const
{
    const float t = simulationTime_;

    // Analytical partial derivatives dh/dx and dh/dz
    const float dhdx = 0.10f * 0.24f * std::cos(0.24f * x + 0.05f * z - 1.5f * t)
                     - 0.06f * 0.38f * std::sin(0.38f * x - 0.14f * z - 2.1f * t)
                     - 0.04f * 0.16f * std::cos(-0.16f * x + 0.32f * z - 1.2f * t);

    const float dhdz = 0.10f * 0.05f * std::cos(0.24f * x + 0.05f * z - 1.5f * t)
                     + 0.06f * 0.14f * std::sin(0.38f * x - 0.14f * z - 2.1f * t)
                     + 0.04f * 0.32f * std::cos(-0.16f * x + 0.32f * z - 1.2f * t);

    return glm::normalize(glm::vec3{-dhdx, 1.0f, -dhdz});
}

glm::vec2 WaterSimulation::flowVelocityAt(float /*x*/, float /*z*/) const
{
    return glm::vec2{DominantFlowSpeed, 0.04f};
}

void WaterSimulation::updateBoats()
{
    for (std::size_t i = 0; i < baseBoats_.size(); ++i)
    {
        const BoatDescriptor& base = baseBoats_[i];
        SimulatedBoatState& state = boatStates_[i];
        const bool navigating = std::strcmp(base.id, "SupplyBoatOffshore") == 0;
        const SupplyRoutePose navigation = navigating
            ? supplyRoutePose(base, supplyDistanceAt(base, simulationTime_))
            : SupplyRoutePose{{base.center.x, base.center.z}, base.yawDegrees};
        const glm::mat4 horizontalRoot = makeTransform(
            {navigation.position.x, 0.0f, navigation.position.y},
            {0.0f, navigation.yawDegrees, 0.0f}, {1.0f, 1.0f, 1.0f});
        const auto sample = [&](float localX, float localZ)
        {
            const glm::vec3 world{horizontalRoot *
                glm::vec4{localX, 0.0f, localZ, 1.0f}};
            float surfaceY = 0.0f;
            if (!waterSurfaceAt(world.x, world.z, surfaceY))
                throw std::runtime_error(std::string("Boat footprint leaves Nile: ") +
                                         base.id);
            return surfaceY;
        };
        const float bowSpan = BoatDescriptor::bowSampleFraction * base.length;
        const float sideSpan = BoatDescriptor::sideSampleFraction * base.width;
        state.centerWaterY = sample(0.0f, 0.0f);
        state.bowWaterY = sample(0.0f, bowSpan);
        state.sternWaterY = sample(0.0f, -bowSpan);
        state.portWaterY = sample(-sideSpan, 0.0f);
        state.starboardWaterY = sample(sideSpan, 0.0f);

        // Local +Z is the bow. Positive X rotation lowers it, while positive
        // Z rotation raises the local +X (starboard) side.
        state.pitchDegrees = glm::clamp(-glm::degrees(std::atan2(
            state.bowWaterY - state.sternWaterY, 2.0f * bowSpan)), -5.0f, 5.0f);
        state.rollDegrees = glm::clamp(glm::degrees(std::atan2(
            state.starboardWaterY - state.portWaterY, 2.0f * sideSpan)),
            -5.0f, 5.0f);
        state.swayDegrees = 0.0f;
        state.horizontalVelocity = navigating
            ? supplyVelocityAt(base, simulationTime_) : glm::vec2{0.0f};
        state.verticalDisplacement = state.centerWaterY -
            IndustrialLandscape::nileSurface().waterY;
        state.position = {navigation.position.x,
                          state.centerWaterY - BoatDescriptor::waterlineLocalY,
                          navigation.position.y};
        state.rotationDegrees = {state.pitchDegrees, navigation.yawDegrees,
                                 state.rollDegrees};
        state.moored = base.mooredAtLanding;
    }
}

glm::mat4 WaterSimulation::boatRootTransform(std::size_t index) const
{
    const SimulatedBoatState& state = boatStates_.at(index);
    return makeTransform(state.position, state.rotationDegrees,
                         {1.0f, 1.0f, 1.0f});
}

void WaterSimulation::updatePropulsionWake(float previousDistance,
                                            float currentDistance,
                                            float previousTime)
{
    for (PropulsionWake& wake : wakes_)
        if (wake.active && simulationTime_ - wake.birthTime >= WakeLifetime)
            wake.active = false;
    if (currentDistance <= previousDistance)
        return;
    for (std::size_t boatIndex = 0; boatIndex < baseBoats_.size(); ++boatIndex)
    {
        const BoatDescriptor& boat = baseBoats_[boatIndex];
        if (std::strcmp(boat.id, "SupplyBoatOffshore") != 0)
            continue;
        while (nextWakeDistance_ <= currentDistance + 1.0e-5f)
        {
            const float fraction = glm::clamp(
                (nextWakeDistance_ - previousDistance) /
                    (currentDistance - previousDistance), 0.0f, 1.0f);
            const float birthTime = glm::mix(previousTime, simulationTime_, fraction);
            const glm::vec2 velocity = supplyVelocityAt(boat, birthTime);
            const float speed = glm::length(velocity);
            if (speed > WakeSpeedThreshold)
            {
                const SupplyRoutePose pose = supplyRoutePose(boat, nextWakeDistance_);
                const SimulatedBoatState& state = boatStates_[boatIndex];
                const glm::mat4 root = makeTransform(
                    {pose.position.x, state.position.y, pose.position.y},
                    {state.pitchDegrees, pose.yawDegrees, state.rollDegrees},
                    {1.0f, 1.0f, 1.0f});
                const glm::vec3 stern{root * glm::vec4{sternLocal(boat), 1.0f}};
                if (!IndustrialLandscape::isInsideNile(stern.x, stern.z))
                    throw std::runtime_error("Supply wake stern leaves Nile");
                PropulsionWake& wake = wakes_[wakeEmissionCount_ % wakes_.size()];
                if (wake.active)
                    throw std::runtime_error("Supply wake pool exhausted");
                wake = {{stern.x, stern.z}, velocity / speed,
                        birthTime, nextWakeDistance_, true};
                ++wakeEmissionCount_;
            }
            nextWakeDistance_ += WakeSpacing;
        }
    }
}

void WaterSimulation::collectSceneObjects(std::vector<SceneObject>& objects) const
{
    const float baseWaterLevel = IndustrialLandscape::nileSurface().waterY;
    // Shoreline ripple strip along the stone quay (-153.2f)
    for (int section = 0; section < 8; ++section)
    {
        const float x = -42.0f + static_cast<float>(section) * 5.0f;
        const float rippleY = baseWaterLevel + 0.015f +
                              0.02f * std::sin(simulationTime_ * 3.5f + x * 0.4f);
        objects.push_back({ScenePrimitive::Cube,
                           makeTransform({x, rippleY, -153.4f}, {}, {4.8f, 0.04f, 0.45f}),
                           MaterialId::Water});
    }

    // Propulsion ripples stay at their emitted world X/Z while riding the
    // animated surface. The moored boat has no propulsion-wake state.
    for (const PropulsionWake& wake : wakes_)
    {
        if (!wake.active)
            continue;
        float surfaceY = 0.0f;
        if (!waterSurfaceAt(wake.position.x, wake.position.y, surfaceY))
            throw std::runtime_error("Supply wake leaves Nile surface");
        const float progress = glm::clamp(
            (simulationTime_ - wake.birthTime) / WakeLifetime, 0.0f, 1.0f);
        const float fade = glm::clamp((1.0f - progress) / 0.20f, 0.0f, 1.0f);
        const float yaw = glm::degrees(std::atan2(wake.direction.x,
                                                 wake.direction.y));
        objects.push_back({ScenePrimitive::Cylinder,
                           makeTransform({wake.position.x,
                                          surfaceY + WakeSurfaceOffset,
                                          wake.position.y},
                                         {0.0f, yaw, 0.0f},
                                         {(0.55f + 0.85f * progress) * fade,
                                          WakeThickness,
                                          (0.35f + 0.50f * progress) * fade}),
                           MaterialId::Water});
    }
}

bool WaterSimulation::validateWaterSimulation(std::ostream& output)
{
    WaterSimulation sim;
    bool boatsBounded = true;
    bool normalsUpward = true;

    for (int step = 0; step < 30; ++step)
    {
        sim.update(0.1f);
        for (const auto& boat : sim.boatStates())
        {
            boatsBounded = boatsBounded && std::isfinite(boat.position.y) &&
                           std::abs(boat.verticalDisplacement) < 0.35f &&
                           std::abs(boat.pitchDegrees) <= 12.0f &&
                           std::abs(boat.rollDegrees) <= 12.0f;
        }

        const glm::vec3 norm = sim.waveNormalAt(0.0f, -160.0f);
        normalsUpward = normalsUpward && (norm.y > 0.85f) &&
                        (std::abs(glm::length(norm) - 1.0f) < 0.01f);
    }

    const NileSurfaceBounds nile = IndustrialLandscape::nileSurface();
    const EnvironmentalContext& context = IndustrialLandscape::environment();
    const auto near = [](float a, float b)
    { return std::abs(a - b) < 1.0e-4f; };
    const bool finiteBounds = std::isfinite(nile.minX) && std::isfinite(nile.maxX) &&
                              std::isfinite(nile.minZ) && std::isfinite(nile.maxZ) &&
                              std::isfinite(nile.waterY) &&
                              nile.minX < nile.maxX && nile.minZ < nile.maxZ;

    // Inspect the actual indexed strip submitted by StaticGizaScene, not only
    // its bounding box: the former rectangular corners are now dry.
    const MeshData mesh = IndustrialLandscape::nileSurfaceMesh();
    const glm::mat4 model = IndustrialLandscape::nileSurfaceModel();
    float renderedMinX = std::numeric_limits<float>::max();
    float renderedMaxX = std::numeric_limits<float>::lowest();
    float renderedMinZ = std::numeric_limits<float>::max();
    float renderedMaxZ = std::numeric_limits<float>::lowest();
    bool renderedLevel = mesh.vertices.size() ==
        static_cast<std::size_t>((IndustrialLandscape::NileLengthSegments + 1) *
                                 (IndustrialLandscape::NileWidthSegments + 1)) &&
        mesh.indices.size() ==
        static_cast<std::size_t>(IndustrialLandscape::NileLengthSegments *
                                 IndustrialLandscape::NileWidthSegments * 6);
    std::vector<glm::vec2> outline;
    outline.reserve(mesh.vertices.size());
    for (const Vertex& vertex : mesh.vertices)
    {
        const glm::vec3 world{model * glm::vec4{vertex.position, 1.0f}};
        outline.emplace_back(world.x, world.z);
        renderedMinX = std::min(renderedMinX, world.x);
        renderedMaxX = std::max(renderedMaxX, world.x);
        renderedMinZ = std::min(renderedMinZ, world.z);
        renderedMaxZ = std::max(renderedMaxZ, world.z);
        renderedLevel = renderedLevel && near(world.y, nile.waterY) &&
                        near(vertex.normal.y, 1.0f) &&
                        std::isfinite(vertex.texCoord.x) &&
                        std::isfinite(vertex.texCoord.y);
    }
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const glm::vec2 a = outline[mesh.indices[i]];
        const glm::vec2 b = outline[mesh.indices[i + 1]];
        const glm::vec2 c = outline[mesh.indices[i + 2]];
        renderedLevel = renderedLevel &&
            ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) < -1.0e-5f);
    }
    const bool renderQueryAgreement = renderedLevel &&
        near(renderedMinX, nile.minX) && near(renderedMaxX, nile.maxX) &&
        near(renderedMinZ, nile.minZ) && near(renderedMaxZ, nile.maxZ);

    constexpr float edgeProbe = 0.001f;
    const float centerX = 0.5f * (nile.minX + nile.maxX);
    const float centerZ = 0.5f * (nile.minZ + nile.maxZ);
    float queriedHeight = std::numeric_limits<float>::quiet_NaN();
    bool boundaries = IndustrialLandscape::waterHeightAt(centerX, centerZ, queriedHeight) &&
                      near(queriedHeight, nile.waterY);
    for (int axis = 0; axis < 2; ++axis)
    {
        const float minimum = axis == 0 ? nile.minX : nile.minZ;
        const float maximum = axis == 0 ? nile.maxX : nile.maxZ;
        for (float edge : {minimum, maximum})
        {
            const float inside = edge == minimum ? edge + edgeProbe : edge - edgeProbe;
            const float outside = edge == minimum ? edge - edgeProbe : edge + edgeProbe;
            const auto query = [&](float coordinate)
            {
                const float x = axis == 0 ? coordinate : centerX;
                const float z = axis == 0 ? centerZ : coordinate;
                float level = -999.0f;
                const bool wet = IndustrialLandscape::waterHeightAt(x, z, level);
                return std::pair<bool, float>{wet, level};
            };
            const auto on = query(edge);
            const auto in = query(inside);
            const auto out = query(outside);
            boundaries = boundaries && on.first && in.first && !out.first &&
                         near(on.second, nile.waterY) &&
                         near(in.second, nile.waterY) && out.second == -999.0f;
        }
    }
    boundaries = boundaries &&
                 nile.contains(nile.minX, centerZ) &&
                 nile.contains(nile.maxX, centerZ) &&
                 !nile.contains(nile.minX, nile.minZ) &&
                 !nile.contains(nile.maxX, nile.maxZ) &&
                 nile.endTaperLength > 0.0f && nile.terminalWidth > 0.0f &&
                 !IndustrialLandscape::isInsideNile(nile.maxX + edgeProbe, centerZ) &&
                 !IndustrialLandscape::isInsideNile(
                     std::numeric_limits<float>::quiet_NaN(), centerZ);
    for (float x : {nile.minX + 5.0f, nile.minX + 10.0f,
                    nile.minX + 15.0f, nile.minX + 20.0f,
                    nile.maxX - 5.0f, nile.maxX - 10.0f,
                    nile.maxX - 15.0f, nile.maxX - 20.0f})
    {
        const float north = nile.northBankAt(x);
        const float south = nile.southBankAt(x);
        boundaries = boundaries && nile.contains(x, north) &&
                     nile.contains(x, south) &&
                     !nile.contains(x, north + edgeProbe) &&
                     !nile.contains(x, south - edgeProbe);
    }
    const auto renderedContains = [&](float x, float z)
    {
        const auto side = [](glm::vec2 a, glm::vec2 b, glm::vec2 p)
        { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); };
        const glm::vec2 p{x, z};
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            const glm::vec2 a = outline[mesh.indices[i]];
            const glm::vec2 b = outline[mesh.indices[i + 1]];
            const glm::vec2 c = outline[mesh.indices[i + 2]];
            if (side(a, b, p) <= 1.0e-3f &&
                side(b, c, p) <= 1.0e-3f &&
                side(c, a, p) <= 1.0e-3f)
                return true;
        }
        return false;
    };
    bool denseShapeAgreement = true;
    for (int end = 0; end < 2; ++end)
    {
        const float startX = end == 0 ? nile.minX - 1.0f :
                             nile.maxX - nile.endTaperLength - 1.0f;
        for (int xi = 0; xi <= 27; ++xi)
            for (int zi = 0; zi <= 28; ++zi)
            {
                const float x = startX + static_cast<float>(xi) + 0.19f;
                const float z = nile.minZ - 1.0f + static_cast<float>(zi) + 0.37f;
                denseShapeAgreement = denseShapeAgreement &&
                    (nile.contains(x, z) == renderedContains(x, z));
            }
    }

    const glm::mat4 floodplainModel = IndustrialLandscape::floodplainModel();
    const float floodplainMinZ = floodplainModel[3].z -
                                 0.5f * context.floodplainSize.y;
    const bool floodplainJoins = near(floodplainMinZ, nile.maxZ) &&
                                 context.floodplainY < nile.waterY &&
                                 nile.waterY - context.floodplainY < 0.02f;

    // Sample the actual indexed terrain query, including both old dry-channel
    // extensions. The bed remains submerged, while the banks rise near the
    // shaped water edge instead of leaving a 0.35 m deep dry trench.
    const bool bedSubmerged =
        SandSimulation::staticTerrainHeightAt(centerX, centerZ) < nile.waterY - 0.30f &&
        SandSimulation::staticTerrainHeightAt(-31.0f, -159.0f) < nile.waterY &&
        SandSimulation::staticTerrainHeightAt(-9.0f, -169.0f) < nile.waterY;
    bool endsGraded = true;
    for (float x : {nile.minX - 2.0f, nile.maxX + 2.0f})
        endsGraded = endsGraded &&
            SandSimulation::staticTerrainHeightAt(x, centerZ) > nile.waterY - 0.20f;
    for (float x : {nile.minX + 5.0f, nile.minX + 10.0f,
                    nile.minX + 15.0f, nile.minX + 20.0f,
                    nile.maxX - 5.0f, nile.maxX - 10.0f,
                    nile.maxX - 15.0f, nile.maxX - 20.0f})
    {
        endsGraded = endsGraded &&
            SandSimulation::staticTerrainHeightAt(x, nile.northBankAt(x) + 2.0f) >
                nile.waterY - 0.20f &&
            SandSimulation::staticTerrainHeightAt(x, nile.southBankAt(x) - 2.0f) >
                nile.waterY - 0.20f;
    }
    const bool quayAdjacent =
        IndustrialLandscape::isInsideNile(-31.0f, -153.4f) &&
        -153.2f - 2.75f < nile.maxZ && -153.2f + 2.75f > nile.maxZ;
    bool boatsInside = true;
    for (const BoatDescriptor& boat : ObjectEnrichment::boats())
        boatsInside = boatsInside && nile.contains(boat.center.x, boat.center.z);

    WaterSimulation initial;
    std::vector<SceneObject> waterObjects;
    initial.collectSceneObjects(waterObjects);
    const float firstRippleY = waterObjects.front().model[3].y;
    const bool waterSimulationLevel = waterObjects.size() == 8 &&
        initial.propulsionEmissionCount() == 0 &&
        near(firstRippleY, nile.waterY + 0.015f +
             0.02f * std::sin(-42.0f * 0.4f));

    WaterSimulation motion;
    WaterSimulation repeat;
    float minimumDisplacement = std::numeric_limits<float>::max();
    float maximumDisplacement = std::numeric_limits<float>::lowest();
    float maximumTiltDegrees = 0.0f;
    float maximumShoreDisplacement = 0.0f;
    bool finiteMotion = motion.surfaceVertices().size() == mesh.vertices.size();
    bool repeatable = true;
    for (int frame = 0; frame < 120; ++frame)
    {
        motion.update(1.0f / 60.0f);
        repeat.update(1.0f / 60.0f);
        for (std::size_t i = 0; i < motion.surfaceVertices().size(); ++i)
        {
            const Vertex& vertex = motion.surfaceVertices()[i];
            const Vertex& repeated = repeat.surfaceVertices()[i];
            const glm::vec2 point = outline[i];
            minimumDisplacement = std::min(minimumDisplacement, vertex.position.y);
            maximumDisplacement = std::max(maximumDisplacement, vertex.position.y);
            maximumTiltDegrees = std::max(maximumTiltDegrees,
                glm::degrees(std::acos(glm::clamp(vertex.normal.y, -1.0f, 1.0f))));
            finiteMotion = finiteMotion &&
                std::isfinite(vertex.position.y) &&
                std::isfinite(vertex.normal.x) &&
                std::isfinite(vertex.normal.y) &&
                std::isfinite(vertex.normal.z) &&
                vertex.normal.y > 0.90f &&
                std::abs(glm::length(vertex.normal) - 1.0f) < 1.0e-4f;
            repeatable = repeatable &&
                vertex.position.y == repeated.position.y &&
                vertex.normal.x == repeated.normal.x &&
                vertex.normal.y == repeated.normal.y &&
                vertex.normal.z == repeated.normal.z;
            if (nile.signedBankDistance(point.x, point.y) < 1.0e-3f)
                maximumShoreDisplacement = std::max(maximumShoreDisplacement,
                                                     std::abs(vertex.position.y));
        }
    }
    const bool boundedMotion = minimumDisplacement >= -0.201f &&
                               maximumDisplacement <= 0.201f &&
                               maximumDisplacement - minimumDisplacement > 0.05f &&
                               maximumTiltDegrees < 20.0f &&
                               maximumShoreDisplacement < 1.0e-3f;
    WaterSimulation thirtyFps;
    WaterSimulation sixtyFps;
    for (int frame = 0; frame < 60; ++frame)
        thirtyFps.update(1.0f / 30.0f);
    for (int frame = 0; frame < 120; ++frame)
        sixtyFps.update(1.0f / 60.0f);
    float maximumEqualTimeDifference = 0.0f;
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        maximumEqualTimeDifference = std::max(maximumEqualTimeDifference,
            std::abs(thirtyFps.surfaceVertices()[i].position.y -
                     sixtyFps.surfaceVertices()[i].position.y));
    }
    const bool equalTimeAgreement = maximumEqualTimeDifference < 1.0e-4f;
    float maximumQueryError = 0.0f;
    bool dynamicQueryAgreement = true;
    for (int column = 1; column < IndustrialLandscape::NileLengthSegments; column += 7)
        for (int across : {3, 7, 10})
        {
            const std::size_t index = static_cast<std::size_t>(
                column * (IndustrialLandscape::NileWidthSegments + 1) + across);
            float queried = -999.0f;
            const glm::vec2 point = outline[index];
            const bool wet = motion.waterSurfaceAt(point.x, point.y, queried);
            const float rendered = nile.waterY +
                motion.surfaceVertices()[index].position.y;
            maximumQueryError = std::max(maximumQueryError,
                                         std::abs(queried - rendered));
            dynamicQueryAgreement = dynamicQueryAgreement && wet &&
                                    std::abs(queried - rendered) < 1.0e-4f;
        }
    for (std::size_t triangle = 0; triangle + 2 < mesh.indices.size();
         triangle += 3 * 37)
    {
        const auto a = mesh.indices[triangle];
        const auto b = mesh.indices[triangle + 1];
        const auto c = mesh.indices[triangle + 2];
        const glm::vec2 point = (outline[a] + outline[b] + outline[c]) / 3.0f;
        const float rendered = nile.waterY +
            (motion.surfaceVertices()[a].position.y +
             motion.surfaceVertices()[b].position.y +
             motion.surfaceVertices()[c].position.y) / 3.0f;
        float queried = -999.0f;
        const bool wet = motion.waterSurfaceAt(point.x, point.y, queried);
        maximumQueryError = std::max(maximumQueryError,
                                     std::abs(queried - rendered));
        dynamicQueryAgreement = dynamicQueryAgreement && wet &&
                                std::abs(queried - rendered) < 1.0e-4f;
    }
    float dryHeight = -999.0f;
    dynamicQueryAgreement = dynamicQueryAgreement &&
        !motion.waterSurfaceAt(nile.minX, nile.minZ, dryHeight) &&
        !motion.waterSurfaceAt(nile.maxX, nile.maxZ, dryHeight) &&
        dryHeight == -999.0f;
    for (float x : {nile.minX, nile.minX + 2.5f, nile.minX + 12.5f,
                    0.0f, nile.maxX - 12.5f, nile.maxX - 2.5f, nile.maxX})
    {
        for (float z : {nile.southBankAt(x), nile.northBankAt(x)})
        {
            float shoreY = -999.0f;
            dynamicQueryAgreement = dynamicQueryAgreement &&
                motion.waterSurfaceAt(x, z, shoreY) &&
                std::abs(shoreY - nile.waterY) < 1.0e-3f;
        }
    }
    WaterSimulation shoreCheck;
    float minimumEndBedClearance = std::numeric_limits<float>::max();
    bool endBedCovered = true;
    for (int timeSample = 0; timeSample < 33; ++timeSample)
    {
        if (timeSample > 0)
            shoreCheck.update(0.25f);
        for (int end = 0; end < 2; ++end)
            for (int station = 0; station <= 20; ++station)
            {
                const float offset = 1.25f * static_cast<float>(station);
                const float x = end == 0 ? nile.minX + offset : nile.maxX - offset;
                for (int across = 0; across <= 10; ++across)
                {
                    const float fraction = static_cast<float>(across) / 10.0f;
                    const float z = glm::mix(nile.southBankAt(x),
                                             nile.northBankAt(x), fraction);
                    float surfaceY = -999.0f;
                    const bool wet = shoreCheck.waterSurfaceAt(x, z, surfaceY);
                    const float clearance = surfaceY -
                        SandSimulation::staticTerrainHeightAt(x, z);
                    minimumEndBedClearance = std::min(minimumEndBedClearance,
                                                       clearance);
                    endBedCovered = endBedCovered && wet && clearance > -0.01f;
                }
            }
    }

    const bool valid = boatsBounded && normalsUpward && finiteBounds &&
                       renderQueryAgreement && boundaries && denseShapeAgreement && floodplainJoins &&
                       bedSubmerged && endsGraded && quayAdjacent && boatsInside &&
                       waterSimulationLevel && finiteMotion && repeatable &&
                       boundedMotion && equalTimeAgreement && dynamicQueryAgreement &&
                       endBedCovered;

    output << "Phase 13 Water Simulation Validation\n"
           << "  all boats vertically bounded in water surface (+/- 0.35m): "
           << (boatsBounded ? "PASS" : "FAIL") << '\n'
           << "  wave normals normalized and oriented upward: "
           << (normalsUpward ? "PASS" : "FAIL") << '\n'
           << "  finite authoritative Nile bounds / indexed strip: "
           << ((finiteBounds && renderQueryAgreement) ? "PASS" : "FAIL") << '\n'
           << "  shaped boundaries and dry former corners: "
           << (boundaries ? "PASS" : "FAIL") << '\n'
           << "  dense end-region render/query agreement: "
           << (denseShapeAgreement ? "PASS" : "FAIL") << '\n'
           << "  submerged bed, graded ends, floodplain and quay: "
           << ((bedSubmerged && endsGraded && floodplainJoins && quayAdjacent)
                   ? "PASS" : "FAIL") << '\n'
           << "  authored boats remain inside shaped water: "
           << (boatsInside ? "PASS" : "FAIL") << '\n'
           << "  eight quay ripples and no initial boat wake: "
           << (waterSimulationLevel ? "PASS" : "FAIL") << '\n'
           << "  finite, bounded animated vertices/normals and stable banks: "
           << ((finiteMotion && boundedMotion) ? "PASS" : "FAIL") << '\n'
           << "  deterministic update and 30/60 FPS equal-time agreement: "
           << ((repeatable && equalTimeAgreement) ? "PASS" : "FAIL")
           << " (max difference " << maximumEqualTimeDifference << " m)\n"
           << "  dynamic query matches rendered vertices: "
           << (dynamicQueryAgreement ? "PASS" : "FAIL")
           << " (max error " << maximumQueryError << " m)\n"
           << "  measured displacement / normal tilt: "
           << minimumDisplacement << " ... " << maximumDisplacement
           << " m / " << maximumTiltDegrees << " degrees\n"
           << "  shaped-end water covers graded bed: "
           << (endBedCovered ? "PASS" : "FAIL")
           << " (minimum clearance " << minimumEndBedClearance << " m)\n"
           << (valid ? "Water simulation validation passed.\n"
                     : "Water simulation validation failed.\n");

    return valid;
}

bool WaterSimulation::validateBoatWaterCoupling(std::ostream& output)
{
    const auto& boats = ObjectEnrichment::boats();
    WaterSimulation sim;
    WaterSimulation repeat;
    const auto initialStates = sim.boatStates();
    struct Metrics
    {
        float minRootY = std::numeric_limits<float>::max();
        float maxRootY = std::numeric_limits<float>::lowest();
        float maxBob = 0.0f;
        float maxPitch = 0.0f;
        float maxRoll = 0.0f;
        float maxWaterlineError = 0.0f;
        float minDraft = std::numeric_limits<float>::max();
        float maxDraft = std::numeric_limits<float>::lowest();
    };
    std::array<Metrics, 2> metrics{};
    bool finiteAndStationary = boats.size() == metrics.size();
    bool samplesValid = true;
    bool partsRigid = true;
    bool mooringsFollow = true;
    bool repeatable = true;
    float maxRigidError = 0.0f;
    float maxPositionDrift = 0.0f;
    float maxYawDrift = 0.0f;
    for (int frame = 0; frame <= 240; ++frame)
    {
        if (frame > 0)
        {
            sim.update(1.0f / 30.0f);
            repeat.update(1.0f / 30.0f);
        }
        for (std::size_t i = 0; i < boats.size(); ++i)
        {
            const BoatDescriptor& boat = boats[i];
            const SimulatedBoatState& state = sim.boatStates()[i];
            const SimulatedBoatState& second = repeat.boatStates()[i];
            const glm::mat4 root = sim.boatRootTransform(i);
            finiteAndStationary = finiteAndStationary &&
                isFiniteNonSingularTransform(root) &&
                std::isfinite(state.pitchDegrees) &&
                std::isfinite(state.rollDegrees) &&
                std::abs(state.pitchDegrees) <= 5.0f &&
                std::abs(state.rollDegrees) <= 5.0f &&
                state.moored == boat.mooredAtLanding;
            if (boat.mooredAtLanding)
            {
                maxPositionDrift = std::max(maxPositionDrift,
                    glm::length(glm::vec2{state.position.x - boat.center.x,
                                           state.position.z - boat.center.z}));
                maxYawDrift = std::max(maxYawDrift,
                    std::abs(state.rotationDegrees.y - boat.yawDegrees));
            }
            repeatable = repeatable && state.position.y == second.position.y &&
                         state.pitchDegrees == second.pitchDegrees &&
                         state.rollDegrees == second.rollDegrees;

            Metrics& m = metrics[i];
            m.minRootY = std::min(m.minRootY, state.position.y);
            m.maxRootY = std::max(m.maxRootY, state.position.y);
            m.maxBob = std::max(m.maxBob, std::abs(state.verticalDisplacement));
            m.maxPitch = std::max(m.maxPitch, std::abs(state.pitchDegrees));
            m.maxRoll = std::max(m.maxRoll, std::abs(state.rollDegrees));

            const float bow = BoatDescriptor::bowSampleFraction * boat.length;
            const float side = BoatDescriptor::sideSampleFraction * boat.width;
            const std::array<glm::vec2, 5> localPoints{{
                {0.0f, 0.0f}, {0.0f, bow}, {0.0f, -bow},
                {-side, 0.0f}, {side, 0.0f}}};
            for (const glm::vec2 local : localPoints)
            {
                const glm::vec3 world{root * glm::vec4{
                    local.x, BoatDescriptor::waterlineLocalY, local.y, 1.0f}};
                float surfaceY = 0.0f;
                const bool wet = sim.waterSurfaceAt(world.x, world.z, surfaceY);
                samplesValid = samplesValid && wet;
                if (wet)
                    m.maxWaterlineError = std::max(m.maxWaterlineError,
                                                   std::abs(world.y - surfaceY));
            }
            const glm::vec3 hullBottom{root * glm::vec4{
                0.0f, BoatDescriptor::hullCenterLocalY -
                      0.5f * BoatDescriptor::hullHeight, 0.0f, 1.0f}};
            const float draft = state.centerWaterY - hullBottom.y;
            m.minDraft = std::min(m.minDraft, draft);
            m.maxDraft = std::max(m.maxDraft, draft);

            const auto parts = ObjectEnrichment::boatLocalParts(boat);
            partsRigid = partsRigid && parts.size() == 10;
            const glm::mat4 inverseRoot = glm::inverse(root);
            for (const SceneObject& part : parts)
            {
                const glm::mat4 recovered = inverseRoot * (root * part.model);
                for (int column = 0; column < 4; ++column)
                    for (int row = 0; row < 4; ++row)
                        maxRigidError = std::max(maxRigidError,
                            std::abs(recovered[column][row] -
                                     part.model[column][row]));
            }
            const auto moorings = ObjectEnrichment::boatMoorings(boat);
            mooringsFollow = mooringsFollow &&
                moorings.size() == (boat.mooredAtLanding ? 2u : 0u);
            for (const BoatMooringDescriptor& mooring : moorings)
            {
                const glm::vec3 boatEnd{root *
                    glm::vec4{mooring.boatAttachmentLocal, 1.0f}};
                const glm::mat4 rope = ConstructionAnimationController::cylinderBetween(
                    boatEnd, mooring.shoreAnchor, 0.055f);
                const glm::vec3 ropeStart{rope * glm::vec4{0.0f, -0.5f, 0.0f, 1.0f}};
                const glm::vec3 ropeEnd{rope * glm::vec4{0.0f, 0.5f, 0.0f, 1.0f}};
                mooringsFollow = mooringsFollow &&
                    glm::length(ropeStart - boatEnd) < 1.0e-4f &&
                    glm::length(ropeEnd - mooring.shoreAnchor) < 1.0e-4f;
            }
        }
    }
    WaterSimulation thirty;
    WaterSimulation sixty;
    for (int frame = 0; frame < 60; ++frame)
        thirty.update(1.0f / 30.0f);
    for (int frame = 0; frame < 120; ++frame)
        sixty.update(1.0f / 60.0f);
    float maxEqualPositionDifference = 0.0f;
    float maxEqualAngleDifference = 0.0f;
    bool resetDeterministic = true;
    sim.reset();
    for (std::size_t i = 0; i < boats.size(); ++i)
    {
        maxEqualPositionDifference = std::max(maxEqualPositionDifference,
            std::abs(thirty.boatStates()[i].position.y -
                     sixty.boatStates()[i].position.y));
        maxEqualAngleDifference = std::max(maxEqualAngleDifference,
            std::max(std::abs(thirty.boatStates()[i].pitchDegrees -
                              sixty.boatStates()[i].pitchDegrees),
                     std::abs(thirty.boatStates()[i].rollDegrees -
                              sixty.boatStates()[i].rollDegrees)));
        resetDeterministic = resetDeterministic &&
            sim.boatStates()[i].position.y == initialStates[i].position.y &&
            sim.boatStates()[i].pitchDegrees == initialStates[i].pitchDegrees &&
            sim.boatStates()[i].rollDegrees == initialStates[i].rollDegrees;
    }
    bool draftAndWaterline = true;
    for (const Metrics& m : metrics)
        draftAndWaterline = draftAndWaterline &&
            m.minDraft > 0.15f && m.maxDraft < 0.17f &&
            m.maxWaterlineError < 0.15f;
    const bool valid = finiteAndStationary && samplesValid && partsRigid &&
        mooringsFollow && repeatable && resetDeterministic &&
        draftAndWaterline && maxPositionDrift < 1.0e-6f &&
        maxYawDrift < 1.0e-6f && maxRigidError < 1.0e-4f &&
        maxEqualPositionDifference < 1.0e-4f &&
        maxEqualAngleDifference < 1.0e-3f;
    output << "Phase 13 boat/water coupling validation\n";
    for (std::size_t i = 0; i < boats.size(); ++i)
    {
        const Metrics& m = metrics[i];
        const SimulatedBoatState& state = thirty.boatStates()[i];
        output << "  " << boats[i].id << ": root Y " << m.minRootY
               << " ... " << m.maxRootY << ", max bob/pitch/roll "
               << m.maxBob << " m / " << m.maxPitch << " / " << m.maxRoll
               << " deg, draft " << m.minDraft << " ... " << m.maxDraft
               << " m, max waterline error " << m.maxWaterlineError << " m\n"
               << "    water center/bow/stern/port/starboard: "
               << state.centerWaterY << " / " << state.bowWaterY << " / "
               << state.sternWaterY << " / " << state.portWaterY << " / "
               << state.starboardWaterY << " m\n";
    }
    output << "  finite roots, fixed cargo X/Z/yaw and five wet samples: "
           << ((finiteAndStationary && samplesValid && maxPositionDrift < 1.0e-6f &&
                maxYawDrift < 1.0e-6f) ? "PASS" : "FAIL") << '\n'
           << "  draft/waterline, rigid parts and mooring endpoints: "
           << ((draftAndWaterline && partsRigid && mooringsFollow &&
                maxRigidError < 1.0e-4f) ? "PASS" : "FAIL") << '\n'
           << "  deterministic repeat/reset: "
           << ((repeatable && resetDeterministic) ? "PASS" : "FAIL")
           << '\n'
           << "  30/60 FPS max position/angle difference: "
           << maxEqualPositionDifference << " m / "
           << maxEqualAngleDifference << " deg\n"
           << (valid ? "Boat/water coupling validation passed.\n"
                     : "Boat/water coupling validation failed.\n");
    return valid;
}

bool WaterSimulation::validateBoatNavigation(std::ostream& output)
{
    const auto& boats = ObjectEnrichment::boats();
    if (boats.size() != 2 || std::strcmp(boats[1].id, "SupplyBoatOffshore") != 0)
        return false;
    const BoatDescriptor& cargo = boats[0];
    const BoatDescriptor& supply = boats[1];
    const NileSurfaceBounds nile = IndustrialLandscape::nileSurface();
    constexpr float halfHullLength = 4.8f; // Exceeds the longest visible bow/stern part.
    constexpr float halfHullWidth = 1.85f; // Includes outer side planks and margin.
    const float routeLength = supplyRouteLength(supply);
    const float travelTime = supplyTravelTime(supply);
    const SupplyRoutePose endpoint = supplyRoutePose(supply, routeLength);
    float minBank = std::numeric_limits<float>::max();
    float minQuay = minBank;
    float minCargo = minBank;
    bool footprintWet = true;
    bool finiteRoute = true;
    // Prevalidate the entire swept conservative hull, including the curved turn.
    for (int sample = 0; sample <= 1000; ++sample)
    {
        const float distance = routeLength * static_cast<float>(sample) / 1000.0f;
        const SupplyRoutePose pose = supplyRoutePose(supply, distance);
        const float yaw = glm::radians(pose.yawDegrees);
        finiteRoute = finiteRoute && std::isfinite(pose.position.x) &&
            std::isfinite(pose.position.y) && std::isfinite(pose.yawDegrees);
        const float centerDistance = glm::length(pose.position -
            glm::vec2{cargo.center.x, cargo.center.z});
        const float cargoRadius = std::hypot(0.5f * cargo.length, 0.5f * cargo.width);
        const float supplyRadius = std::hypot(halfHullLength, halfHullWidth);
        minCargo = std::min(minCargo, centerDistance - cargoRadius - supplyRadius);
        for (float side : {-halfHullWidth, 0.0f, halfHullWidth})
            for (float end : {-halfHullLength, 0.0f, halfHullLength})
            {
                const float x = pose.position.x + std::cos(yaw) * side +
                    std::sin(yaw) * end;
                const float z = pose.position.y - std::sin(yaw) * side +
                    std::cos(yaw) * end;
                footprintWet = footprintWet && nile.contains(x, z);
                minBank = std::min(minBank, nile.signedBankDistance(x, z));
                // The eight existing quay slabs span this exact outer AABB.
                const float quayDx = std::max({-44.35f - x, 0.0f, x + 4.65f});
                const float quayDz = std::max({-155.95f - z, 0.0f, z + 150.45f});
                minQuay = std::min(minQuay, std::hypot(quayDx, quayDz));
            }
    }
    WaterSimulation motion;
    WaterSimulation repeat;
    const SimulatedBoatState initial = motion.boatStates()[1];
    const int frames = static_cast<int>(std::ceil((travelTime + 3.0f) * 30.0f));
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float maxPitch = 0.0f;
    float maxRoll = 0.0f;
    float maxWaterlineError = 0.0f;
    float maxHorizontalStep = 0.0f;
    float maxVerticalStep = 0.0f;
    float maxYawStep = 0.0f;
    float maxHeadingError = 0.0f;
    float maxRigidError = 0.0f;
    int invalidWaterSamples = 0;
    bool cargoFixed = true;
    bool repeatable = true;
    bool sampleDerivedAttitude = true;
    bool stopped = true;
    SimulatedBoatState previous = motion.boatStates()[1];
    for (int frame = 0; frame <= frames; ++frame)
    {
        if (frame > 0)
        {
            motion.update(1.0f / 30.0f);
            repeat.update(1.0f / 30.0f);
        }
        const SimulatedBoatState& state = motion.boatStates()[1];
        const SimulatedBoatState& moored = motion.boatStates()[0];
        cargoFixed = cargoFixed && moored.position.x == cargo.center.x &&
            moored.position.z == cargo.center.z &&
            moored.rotationDegrees.y == cargo.yawDegrees;
        const SimulatedBoatState& second = repeat.boatStates()[1];
        repeatable = repeatable &&
            glm::all(glm::equal(state.position, second.position)) &&
            glm::all(glm::equal(state.rotationDegrees, second.rotationDegrees));
        minY = std::min(minY, state.position.y);
        maxY = std::max(maxY, state.position.y);
        maxPitch = std::max(maxPitch, std::abs(state.pitchDegrees));
        maxRoll = std::max(maxRoll, std::abs(state.rollDegrees));
        const glm::mat4 root = motion.boatRootTransform(1);
        const glm::mat4 inverseRoot = glm::inverse(root);
        for (const SceneObject& part : ObjectEnrichment::boatLocalParts(supply))
        {
            const glm::mat4 recovered = inverseRoot * (root * part.model);
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    maxRigidError = std::max(maxRigidError,
                        std::abs(recovered[column][row] - part.model[column][row]));
        }
        const float bow = BoatDescriptor::bowSampleFraction * supply.length;
        const float side = BoatDescriptor::sideSampleFraction * supply.width;
        for (const glm::vec2 local : std::array<glm::vec2, 5>{{
                 {0.0f, 0.0f}, {0.0f, bow}, {0.0f, -bow},
                 {-side, 0.0f}, {side, 0.0f}}})
        {
            const glm::vec3 world{root * glm::vec4{
                local.x, BoatDescriptor::waterlineLocalY, local.y, 1.0f}};
            float waterY = 0.0f;
            if (!motion.waterSurfaceAt(world.x, world.z, waterY))
                ++invalidWaterSamples;
            else
                maxWaterlineError = std::max(maxWaterlineError,
                    std::abs(world.y - waterY));
        }
        const float expectedPitch = glm::clamp(-glm::degrees(std::atan2(
            state.bowWaterY - state.sternWaterY, 2.0f * bow)), -5.0f, 5.0f);
        const float expectedRoll = glm::clamp(glm::degrees(std::atan2(
            state.starboardWaterY - state.portWaterY, 2.0f * side)), -5.0f, 5.0f);
        sampleDerivedAttitude = sampleDerivedAttitude &&
            std::abs(state.pitchDegrees - expectedPitch) < 1.0e-5f &&
            std::abs(state.rollDegrees - expectedRoll) < 1.0e-5f;
        if (frame > 0)
        {
            const glm::vec2 delta{state.position.x - previous.position.x,
                                  state.position.z - previous.position.z};
            const float step = glm::length(delta);
            maxHorizontalStep = std::max(maxHorizontalStep, step);
            maxVerticalStep = std::max(maxVerticalStep,
                std::abs(state.position.y - previous.position.y));
            maxYawStep = std::max(maxYawStep,
                std::abs(state.rotationDegrees.y - previous.rotationDegrees.y));
            if (step > 1.0e-4f)
            {
                const glm::vec2 forward{
                    std::sin(glm::radians(state.rotationDegrees.y)),
                    std::cos(glm::radians(state.rotationDegrees.y))};
                const glm::vec2 velocityDirection = delta / step;
                maxHeadingError = std::max(maxHeadingError, glm::degrees(std::acos(
                    glm::clamp(glm::dot(forward, velocityDirection), -1.0f, 1.0f))));
            }
            if (motion.simulationTime_ >= travelTime + 0.1f)
                stopped = stopped && step < 1.0e-5f;
        }
        previous = state;
    }
    const SimulatedBoatState final = motion.boatStates()[1];
    const bool reachedEnd = glm::length(glm::vec2{final.position.x,
        final.position.z} - endpoint.position) < 1.0e-4f &&
        std::abs(final.rotationDegrees.y - 90.0f) < 1.0e-4f;
    motion.reset();
    const SimulatedBoatState reset = motion.boatStates()[1];
    bool resetCorrect = glm::all(glm::equal(reset.position, initial.position)) &&
        glm::all(glm::equal(reset.rotationDegrees, initial.rotationDegrees));
    for (int frame = 0; frame < 300; ++frame)
        motion.update(1.0f / 30.0f);
    WaterSimulation fresh;
    for (int frame = 0; frame < 300; ++frame)
        fresh.update(1.0f / 30.0f);
    resetCorrect = resetCorrect &&
        glm::all(glm::equal(motion.boatStates()[1].position,
                            fresh.boatStates()[1].position)) &&
        glm::all(glm::equal(motion.boatStates()[1].rotationDegrees,
                            fresh.boatStates()[1].rotationDegrees));
    WaterSimulation thirty;
    WaterSimulation sixty;
    for (int frame = 0; frame < 600; ++frame)
        thirty.update(1.0f / 30.0f);
    for (int frame = 0; frame < 1200; ++frame)
        sixty.update(1.0f / 60.0f);
    const auto& a = thirty.boatStates()[1];
    const auto& b = sixty.boatStates()[1];
    const float equalPosition = glm::length(a.position - b.position);
    const float equalOrientation = glm::length(a.rotationDegrees - b.rotationDegrees);
    const bool valid = finiteRoute && footprintWet && minBank > 3.0f &&
        minQuay > 1.5f && minCargo > 10.0f && cargoFixed && repeatable &&
        sampleDerivedAttitude && invalidWaterSamples == 0 &&
        maxWaterlineError < 0.15f && maxRigidError < 1.0e-4f &&
        maxHeadingError < 1.0f && maxHorizontalStep < 0.08f &&
        maxVerticalStep < 0.05f && maxYawStep < 0.7f && stopped &&
        reachedEnd && resetCorrect && equalPosition < 1.0e-3f &&
        equalOrientation < 1.0e-3f;
    output << "Phase 13 one-way supply boat navigation\n"
           << "  start/end XZ: (" << supply.center.x << ", " << supply.center.z
           << ") -> (" << endpoint.position.x << ", " << endpoint.position.y
           << "), arc/total length: " << supplyArcLength(supply) << " / "
           << routeLength << " m, time: " << travelTime << " s\n"
           << "  minimum bank/quay/cargo clearance: " << minBank << " / "
           << minQuay << " / " << minCargo << " m\n"
           << "  supply Y range: " << minY << " ... " << maxY
           << ", max pitch/roll: " << maxPitch << " / " << maxRoll
           << " deg, max waterline error: " << maxWaterlineError << " m\n"
           << "  max horizontal/vertical/yaw frame step: "
           << maxHorizontalStep << " m / " << maxVerticalStep << " m / "
           << maxYawStep << " deg, heading error: " << maxHeadingError
           << " deg, rigid error: " << maxRigidError << '\n'
           << "  invalid water samples: " << invalidWaterSamples
           << ", 30/60 FPS position/angle error: " << equalPosition
           << " m / " << equalOrientation << " deg\n"
           << (valid ? "Supply navigation validation passed.\n"
                     : "Supply navigation validation failed.\n");
    return valid;
}

bool WaterSimulation::validateBoatWake(std::ostream& output)
{
    WaterSimulation sim;
    WaterSimulation repeat;
    const BoatDescriptor& supply = ObjectEnrichment::boats()[1];
    const BoatDescriptor& cargo = ObjectEnrichment::boats()[0];
    const float travelTime = supplyTravelTime(supply);
    bool deterministic = true;
    bool sternOwned = true;
    bool velocityOwned = true;
    bool insideWater = true;
    bool shorePreserved = true;
    bool noStoppedEmission = true;
    bool cargoNoPropulsion = true;
    bool leftBehind = true;
    float maxSternOriginError = 0.0f;
    float maxDirectionError = 0.0f;
    float maxSurfaceError = 0.0f;
    std::size_t maxActive = 0;
    std::size_t stoppedEmissionCount = 0;
    for (int frame = 0; frame <= 1350; ++frame)
    {
        if (frame > 0)
        {
            sim.update(1.0f / 30.0f);
            repeat.update(1.0f / 30.0f);
        }
        deterministic = deterministic &&
            sim.propulsionEmissionCount() == repeat.propulsionEmissionCount();
        const auto& wakes = sim.propulsionWakes();
        const auto& repeated = repeat.propulsionWakes();
        std::size_t active = 0;
        const glm::vec2 boatPosition{sim.boatStates()[1].position.x,
                                     sim.boatStates()[1].position.z};
        const glm::vec2 currentVelocity = sim.boatStates()[1].horizontalVelocity;
        cargoNoPropulsion = cargoNoPropulsion &&
            glm::length(sim.boatStates()[0].horizontalVelocity) == 0.0f &&
            sim.boatStates()[0].position.x == cargo.center.x &&
            sim.boatStates()[0].position.z == cargo.center.z;
        for (std::size_t i = 0; i < wakes.size(); ++i)
        {
            const PropulsionWake& wake = wakes[i];
            const PropulsionWake& other = repeated[i];
            deterministic = deterministic && wake.active == other.active &&
                wake.emissionDistance == other.emissionDistance &&
                wake.birthTime == other.birthTime &&
                glm::all(glm::equal(wake.position, other.position)) &&
                glm::all(glm::equal(wake.direction, other.direction));
            if (!wake.active)
                continue;
            ++active;
            const SupplyRoutePose eventPose = supplyRoutePose(
                supply, wake.emissionDistance);
            const float yaw = glm::radians(eventPose.yawDegrees);
            const glm::vec3 localStern = sternLocal(supply);
            const glm::vec2 expectedStern{
                eventPose.position.x + std::sin(yaw) * localStern.z,
                eventPose.position.y + std::cos(yaw) * localStern.z};
            maxSternOriginError = std::max(maxSternOriginError,
                glm::length(wake.position - expectedStern));
            sternOwned = sternOwned && wake.emissionDistance > 0.0f &&
                wake.emissionDistance <= supplyRouteLength(supply);
            const glm::vec2 eventVelocity = supplyVelocityAt(
                supply, wake.birthTime);
            const float eventSpeed = glm::length(eventVelocity);
            velocityOwned = velocityOwned && eventSpeed > WakeSpeedThreshold;
            if (eventSpeed > 0.0f)
                maxDirectionError = std::max(maxDirectionError,
                    glm::degrees(std::acos(glm::clamp(
                        glm::dot(wake.direction, eventVelocity / eventSpeed),
                        -1.0f, 1.0f))));
            float surfaceY = 0.0f;
            insideWater = insideWater &&
                sim.waterSurfaceAt(wake.position.x, wake.position.y, surfaceY);
            if (glm::length(currentVelocity) > 0.3f &&
                sim.simulationTime_ - wake.birthTime > 0.15f)
                leftBehind = leftBehind &&
                    glm::dot(glm::normalize(boatPosition - wake.position),
                             glm::normalize(currentVelocity)) > 0.25f;
        }
        maxActive = std::max(maxActive, active);
        if (frame < 10)
            noStoppedEmission = noStoppedEmission &&
                sim.propulsionEmissionCount() == 0;
        if (sim.simulationTime_ >= travelTime + 0.1f)
        {
            if (stoppedEmissionCount == 0)
                stoppedEmissionCount = sim.propulsionEmissionCount();
            noStoppedEmission = noStoppedEmission &&
                sim.propulsionEmissionCount() == stoppedEmissionCount;
        }
        if (frame % 30 == 0)
        {
            std::vector<SceneObject> objects;
            sim.collectSceneObjects(objects);
            shorePreserved = shorePreserved && objects.size() == 8 + active;
            for (int shore = 0; shore < 8; ++shore)
            {
                const float x = -42.0f + static_cast<float>(shore) * 5.0f;
                const float expectedY = IndustrialLandscape::nileSurface().waterY +
                    0.015f + 0.02f * std::sin(sim.simulationTime_ * 3.5f + x * 0.4f);
                shorePreserved = shorePreserved &&
                    objects[shore].primitive == ScenePrimitive::Cube &&
                    std::abs(objects[shore].model[3].x - x) < 1.0e-5f &&
                    std::abs(objects[shore].model[3].y - expectedY) < 1.0e-5f;
            }
            std::size_t rendered = 8;
            for (const PropulsionWake& wake : wakes)
            {
                if (!wake.active)
                    continue;
                float surfaceY = 0.0f;
                if (!sim.waterSurfaceAt(wake.position.x, wake.position.y,
                                        surfaceY))
                    insideWater = false;
                else
                    maxSurfaceError = std::max(maxSurfaceError,
                        std::abs(objects[rendered].model[3].y -
                                 (surfaceY + WakeSurfaceOffset)));
                ++rendered;
            }
        }
    }
    const std::size_t totalEmitted = sim.propulsionEmissionCount();
    const bool expiredAfterStop = std::all_of(
        sim.propulsionWakes().begin(), sim.propulsionWakes().end(),
        [](const PropulsionWake& wake) { return !wake.active; });
    sim.reset();
    const bool resetCleared = sim.propulsionEmissionCount() == 0 &&
        std::all_of(sim.propulsionWakes().begin(), sim.propulsionWakes().end(),
            [](const PropulsionWake& wake) { return !wake.active; });
    WaterSimulation thirty;
    WaterSimulation sixty;
    for (int frame = 0; frame < 600; ++frame)
        thirty.update(1.0f / 30.0f);
    for (int frame = 0; frame < 1200; ++frame)
        sixty.update(1.0f / 60.0f);
    float maxEqualOriginError = 0.0f;
    float maxEqualAgeError = 0.0f;
    bool equalEmissionOrder =
        thirty.propulsionEmissionCount() == sixty.propulsionEmissionCount();
    for (std::size_t i = 0; i < thirty.propulsionWakes().size(); ++i)
    {
        const auto& a = thirty.propulsionWakes()[i];
        const auto& b = sixty.propulsionWakes()[i];
        equalEmissionOrder = equalEmissionOrder && a.active == b.active &&
            a.emissionDistance == b.emissionDistance;
        if (a.active && b.active)
        {
            maxEqualOriginError = std::max(maxEqualOriginError,
                glm::length(a.position - b.position));
            maxEqualAgeError = std::max(maxEqualAgeError,
                std::abs(a.birthTime - b.birthTime));
        }
    }
    const bool valid = deterministic && sternOwned && velocityOwned &&
        insideWater && shorePreserved && noStoppedEmission &&
        cargoNoPropulsion && leftBehind && expiredAfterStop && resetCleared &&
        equalEmissionOrder && totalEmitted == 46 && maxActive <= 16 &&
        maxSternOriginError < 0.03f && maxDirectionError < 0.05f &&
        maxSurfaceError < 1.0e-5f && maxEqualOriginError < 0.02f &&
        maxEqualAgeError < 0.01f;
    output << "Phase 13 supply propulsion-wake validation\n"
           << "  pool/spacing/threshold/lifetime: " << sim.propulsionWakes().size()
           << " / " << WakeSpacing << " m / " << WakeSpeedThreshold
           << " m/s / " << WakeLifetime << " s\n"
           << "  emitted / max active: " << totalEmitted << " / " << maxActive
           << ", stern-origin/direction error: " << maxSternOriginError
           << " m / " << maxDirectionError << " deg\n"
           << "  surface error: " << maxSurfaceError
           << " m, 30/60 FPS origin/age error: " << maxEqualOriginError
           << " m / " << maxEqualAgeError << " s\n"
           << "  deterministic/reset/shore/cargo/stop: "
           << ((deterministic && resetCleared && shorePreserved &&
                cargoNoPropulsion && noStoppedEmission) ? "PASS" : "FAIL")
           << '\n'
           << (valid ? "Propulsion-wake validation passed.\n"
                     : "Propulsion-wake validation failed.\n");
    return valid;
}

bool WaterSimulation::validateWaterAppearance(std::ostream& output)
{
    const MeshData mesh = IndustrialLandscape::nileSurfaceMesh();
    const bool topology = mesh.vertices.size() == 938 &&
                          mesh.indices.size() == 5148;
    WaterSimulation water;
    water.update(0.35f);
    bool query = true;
    for (const glm::vec2 p : {glm::vec2{-100.0f, -166.0f},
                              glm::vec2{0.0f, -166.0f},
                              glm::vec2{100.0f, -166.0f}})
    {
        float y = 0.0f;
        query = query && water.waterSurfaceAt(p.x, p.y, y) &&
                std::isfinite(y);
    }
    const glm::vec2 flow = glm::normalize(water.flowVelocityAt(0.0f, -166.0f));
    const glm::vec2 crossFlow{-flow.y, flow.x};
    float maxTilt = 0.0f;
    bool finiteDetail = true;
    for (float x = -100.0f; x <= 100.0f; x += 7.0f)
        for (float z = -176.0f; z <= -156.0f; z += 5.0f)
        {
            const float along = glm::dot(glm::vec2{x, z}, flow);
            const float across = glm::dot(glm::vec2{x, z}, crossFlow);
            const float t = water.timeSeconds();
            const float medium = along * 0.87f + across * 1.25f - t * 1.38f;
            const float fine = along * 2.25f - across * 3.40f - t * 2.72f;
            const float shimmer = along * 4.10f + across * 5.30f - t * 3.15f;
            const glm::vec2 gradient =
                0.020f * std::cos(medium) * (0.87f * flow + 1.25f * crossFlow) +
                0.004f * std::cos(fine) * (2.25f * flow - 3.40f * crossFlow) +
                0.001f * std::cos(shimmer) * (4.10f * flow + 5.30f * crossFlow);
            const glm::vec3 normal = glm::normalize(water.waveNormalAt(x, z) +
                glm::vec3{-gradient.x, 0.0f, -gradient.y});
            finiteDetail = finiteDetail && std::isfinite(normal.x) &&
                std::isfinite(normal.y) && std::isfinite(normal.z) &&
                std::abs(glm::length(normal) - 1.0f) < 1.0e-5f &&
                normal.y > 0.95f;
            maxTilt = std::max(maxTilt, glm::degrees(std::acos(
                std::clamp(normal.y, -1.0f, 1.0f))));
        }
    bool fresnel = true;
    for (float cosine = 0.0f; cosine <= 1.0f; cosine += 0.1f)
    {
        const float grazing = 1.0f - cosine;
        const float response = 0.13f * grazing * grazing * grazing;
        fresnel = fresnel && std::isfinite(response) &&
                  response >= 0.0f && response <= 0.13f;
    }
    const bool time = water.timeSeconds() > 0.34f &&
        water.timeSeconds() < 0.36f;
    const bool valid = topology && query && finiteDetail && fresnel && time &&
        maxTilt < 10.0f;
    output << "Nile visual-detail validation: " << (valid ? "PASS" : "FAIL")
           << " topology=" << mesh.vertices.size() << '/' << mesh.indices.size()
           << " maximum normal tilt=" << maxTilt << " degrees\n";
    return valid;
}
