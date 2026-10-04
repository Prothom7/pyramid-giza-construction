#include "scene/WaterSimulation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "scene/IndustrialLandscape.h"
#include "scene/SandSimulation.h"
#include "graphics/Mesh.h"



WaterSimulation::WaterSimulation()
{
    initBoats();
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
    updateBoats();
    updateSurface();
}

void WaterSimulation::update(float deltaTime)
{
    if (!enabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    simulationTime_ += deltaTime;
    updateBoats();
    updateSurface();
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

        const float wave = waveHeightAt(base.center.x, base.center.z);
        state.verticalDisplacement = wave;

        // Slopes for pitch and roll
        const float dx = (waveHeightAt(base.center.x + 1.0f, base.center.z) -
                          waveHeightAt(base.center.x - 1.0f, base.center.z)) * 0.5f;
        const float dz = (waveHeightAt(base.center.x, base.center.z + 1.0f) -
                          waveHeightAt(base.center.x, base.center.z - 1.0f)) * 0.5f;

        state.pitchDegrees = glm::clamp(std::atan2(dz, 1.0f) * 57.29578f * 1.5f, -8.0f, 8.0f);
        state.rollDegrees = glm::clamp(std::atan2(dx, 1.0f) * 57.29578f * 1.5f, -8.0f, 8.0f);
        state.swayDegrees = std::sin(simulationTime_ * 1.2f + static_cast<float>(i)) * 2.2f;

        state.position = base.center + glm::vec3{0.0f, wave, 0.0f};
        state.rotationDegrees = glm::vec3{state.pitchDegrees,
                                          base.yawDegrees + state.swayDegrees,
                                          state.rollDegrees};
        state.wakeOrigin = base.center + glm::vec3{base.length * 0.45f, wave, 0.0f};
        state.moored = base.mooredAtLanding;
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

    // Dynamic boat wake ripples
    for (const auto& boat : boatStates_)
    {
        for (int ring = 1; ring <= 3; ++ring)
        {
            const float progress = std::fmod(simulationTime_ * 0.8f + ring * 0.33f, 1.0f);
            const float radius = 1.0f + progress * 3.5f;
            const float wakeY = baseWaterLevel + 0.01f + (1.0f - progress) * 0.025f;

            objects.push_back({ScenePrimitive::Cylinder,
                               makeTransform(boat.wakeOrigin + glm::vec3{progress * 1.5f, wakeY - boat.wakeOrigin.y, 0.0f},
                                             {}, {radius, 0.02f, radius * 0.6f}),
                               MaterialId::Water});
        }
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
    const bool waterSimulationLevel = waterObjects.size() == 14 &&
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
           << "  ripple objects use the same water level: "
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
