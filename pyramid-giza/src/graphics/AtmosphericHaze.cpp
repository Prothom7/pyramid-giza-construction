#include "graphics/AtmosphericHaze.h"

#include <algorithm>
#include <cmath>
#include <ostream>

#include "graphics/SkyBackground.h"
#include "scene/SandSimulation.h"

namespace
{
float smoothFraction(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

float AtmosphericHaze::factor(const glm::vec3& camera,
                              const glm::vec3& fragment)
{
    const float horizontalDistance = glm::length(glm::vec2{
        fragment.x - camera.x, fragment.z - camera.z});
    const float t = std::clamp((horizontalDistance - startDistance) /
                               (endDistance - startDistance), 0.0f, 1.0f);
    return maximumBlend * smoothFraction(t);
}

float AtmosphericHaze::terrainFactor(const glm::vec3& camera,
                                     const glm::vec3& fragment)
{
    const float distanceXZ = glm::length(glm::vec2{
        fragment.x - camera.x, fragment.z - camera.z});
    const float edgeFraction = std::min({
        (fragment.x - SandSimulation::WorldMinX) / terrainEdgeWidthX,
        (SandSimulation::WorldMaxX - fragment.x) / terrainEdgeWidthX,
        (fragment.z - SandSimulation::WorldMinZ) / terrainEdgeWidthZ,
        (SandSimulation::WorldMaxZ - fragment.z) / terrainEdgeWidthZ});
    const float edgeFade = 1.0f - smoothFraction(edgeFraction);
    const float distanceGate = smoothFraction((distanceXZ - terrainEdgeGateStart) /
                                              (terrainEdgeGateEnd - terrainEdgeGateStart));
    return std::max(factor(camera, fragment),
                    terrainEdgeMaximumBlend * edgeFade * distanceGate);
}

glm::vec3 AtmosphericHaze::color(const SunState& sun)
{
    return skyColors(sun).horizon;
}

bool validateAtmosphericHaze(std::ostream& output)
{
    const glm::vec3 camera{12.0f, 70.0f, -8.0f};
    const float distances[]{0.0f, 25.0f, 50.0f, 100.0f,
                            150.0f, 200.0f, 300.0f, 400.0f};
    bool valid = true;
    float previous = -1.0f;
    output << "Atmospheric haze validation (horizontal meters / factor):\n";
    for (const float distance : distances)
    {
        const float factor = AtmosphericHaze::factor(
            camera, camera + glm::vec3{distance, 100.0f, 0.0f});
        output << "  " << distance << " / " << factor << '\n';
        valid = valid && std::isfinite(factor) && factor >= 0.0f &&
                factor <= AtmosphericHaze::maximumBlend && factor >= previous;
        previous = factor;
    }
    valid = valid && AtmosphericHaze::factor(camera, camera + glm::vec3{25.0f, 0.0f, 0.0f}) == 0.0f &&
            AtmosphericHaze::factor(camera, camera + glm::vec3{400.0f, 0.0f, 0.0f}) > 0.91f;
    const glm::vec3 nearPoint{40.0f, 0.0f, -8.0f};
    valid = valid && AtmosphericHaze::factor(camera, nearPoint) <
                     AtmosphericHaze::factor(camera + glm::vec3{-150.0f, 0.0f, 0.0f}, nearPoint);
    const glm::vec3 lowCamera{50.0f, 2.5f, 75.0f};
    const glm::vec3 farEdge{SandSimulation::WorldMaxX, 0.0f, 75.0f};
    const float edgeFactor = AtmosphericHaze::terrainFactor(lowCamera, farEdge);
    valid = valid && edgeFactor > 0.99f && edgeFactor <= 1.0f &&
            AtmosphericHaze::terrainFactor(lowCamera, {100.0f, 0.0f, 75.0f}) == 0.0f &&
            AtmosphericHaze::terrainFactor(lowCamera, {140.0f, 0.0f, 75.0f}) < 0.1f &&
            AtmosphericHaze::terrainFactor({190.0f, 2.5f, 75.0f},
                                           {190.0f, 0.0f, 75.0f}) == 0.0f;
    const SunState morning = SunController::evaluate(8.0f);
    const SunState noon = SunController::evaluate(12.0f);
    const SunState evening = SunController::evaluate(17.0f);
    const glm::vec3 m = AtmosphericHaze::color(morning);
    const glm::vec3 n = AtmosphericHaze::color(noon);
    const glm::vec3 e = AtmosphericHaze::color(evening);
    const SunState midnight = SunController::evaluate(0.0f);
    const glm::vec3 night = AtmosphericHaze::color(midnight);
    for (const glm::vec3 color : {m, n, e, night})
        valid = valid && std::isfinite(color.r) && std::isfinite(color.g) &&
                std::isfinite(color.b) && glm::all(glm::greaterThanEqual(color, glm::vec3{0.0f})) &&
                glm::all(glm::lessThanEqual(color, glm::vec3{1.0f}));
    valid = valid && m == skyColors(morning).horizon &&
            n == skyColors(noon).horizon && e == skyColors(evening).horizon &&
            night == skyColors(midnight).horizon &&
            glm::length(night) < glm::length(n) * 0.5f &&
            AtmosphericHaze::color(noon) == n &&
            glm::distance(m, n) > 0.02f && glm::distance(n, e) > 0.02f;
    output << "  low-camera finite-ground edge factor: " << edgeFactor << '\n'
           << "  morning/noon/evening horizon RGB: ("
           << m.r << ", " << m.g << ", " << m.b << ") / ("
           << n.r << ", " << n.g << ", " << n.b << ") / ("
           << e.r << ", " << e.g << ", " << e.b << ")\n"
           << (valid ? "Atmospheric haze validation passed.\n"
                     : "Atmospheric haze validation failed.\n");
    return valid;
}
