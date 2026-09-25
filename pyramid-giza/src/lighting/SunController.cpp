#include "lighting/SunController.h"

#include <algorithm>
#include <cmath>
#include <ostream>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "scene/SceneTypes.h"

namespace
{
bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finiteState(const SunState& state)
{
    return std::isfinite(state.timeOfDay) && finiteVector(state.light.direction) &&
           finiteVector(state.light.color) && std::isfinite(state.light.intensity) &&
           finiteVector(state.ambientColor) && std::isfinite(state.ambientIntensity) &&
           finiteVector(state.skyColor);
}

float wrapDaylight(float hours)
{
    const float span = SunController::daylightEnd - SunController::daylightStart;
    float offset = std::fmod(hours - SunController::daylightStart, span);
    if (offset < 0.0f)
        offset += span;
    return SunController::daylightStart + offset;
}

glm::vec3 daylightBlend(const glm::vec3& morning, const glm::vec3& noon,
                        const glm::vec3& evening, float progress)
{
    if (progress <= 0.5f)
        return glm::mix(morning, noon, progress * 2.0f);
    return glm::mix(noon, evening, (progress - 0.5f) * 2.0f);
}
} // namespace

SunController::SunController()
    : state_(evaluate(morningTime))
{
}

SunState SunController::evaluate(float timeOfDay)
{
    const float time = std::clamp(timeOfDay, daylightStart, daylightEnd);
    const float progress = (time - daylightStart) / (daylightEnd - daylightStart);
    const float solarArc = std::sin(glm::pi<float>() * progress);
    const float elevationDegrees = 12.0f + 66.0f * solarArc;
    const float azimuthDegrees = -105.0f + 210.0f * progress;
    const float elevation = glm::radians(elevationDegrees);
    const float azimuth = glm::radians(azimuthDegrees);

    // toSun points from the scene toward the sun. The directional-light vector stores
    // the opposite ray direction so shaders use -direction as L.
    const glm::vec3 toSun{
        std::cos(elevation) * std::cos(azimuth),
        std::sin(elevation),
        std::cos(elevation) * std::sin(azimuth)
    };

    SunState state;
    state.timeOfDay = time;
    state.light.direction = -glm::normalize(toSun);
    state.light.color = daylightBlend(
        {1.00f, 0.78f, 0.60f}, {1.00f, 0.96f, 0.86f},
        {1.00f, 0.70f, 0.52f}, progress);
    state.light.intensity = 0.72f + 0.38f * solarArc;
    state.ambientColor = daylightBlend(
        {0.78f, 0.74f, 0.70f}, {0.72f, 0.82f, 1.00f},
        {0.76f, 0.69f, 0.68f}, progress);
    state.ambientIntensity = 0.78f + 0.12f * solarArc;
    state.skyColor = daylightBlend(
        {0.62f, 0.67f, 0.76f}, {0.45f, 0.68f, 0.90f},
        {0.72f, 0.56f, 0.48f}, progress);
    return state;
}

void SunController::update(float deltaTime)
{
    if (!automatic_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;
    const float next = state_.timeOfDay + timeScale_ * deltaTime;
    state_ = evaluate(next > daylightEnd ? wrapDaylight(next) : next);
}

void SunController::setTimeOfDay(float hours)
{
    if (!std::isfinite(hours))
        return;
    state_ = evaluate(hours);
}

void SunController::adjustTime(float hours)
{
    if (!std::isfinite(hours))
        return;
    automatic_ = false;
    state_ = evaluate(std::clamp(state_.timeOfDay + hours, daylightStart, daylightEnd));
}

void SunController::setTimeScale(float hoursPerSecond)
{
    if (std::isfinite(hoursPerSecond) && hoursPerSecond > 0.0f)
        timeScale_ = hoursPerSecond;
}

void SunController::selectMorning()
{
    automatic_ = false;
    state_ = evaluate(morningTime);
}

void SunController::selectNoon()
{
    automatic_ = false;
    state_ = evaluate(noonTime);
}

void SunController::selectEvening()
{
    automatic_ = false;
    state_ = evaluate(eveningTime);
}

void SunController::cycleDebugMode()
{
    const int count = static_cast<int>(LightingDebugMode::Count);
    const int next = (static_cast<int>(debugMode_) + 1) % count;
    debugMode_ = static_cast<LightingDebugMode>(next);
}

void SunController::setDebugMode(LightingDebugMode mode)
{
    const int value = static_cast<int>(mode);
    if (value >= 0 && value < static_cast<int>(LightingDebugMode::Count))
        debugMode_ = mode;
}

const char* SunController::debugModeName(LightingDebugMode mode)
{
    switch (mode)
    {
    case LightingDebugMode::Normal: return "Normal Blinn-Phong";
    case LightingDebugMode::DiffuseOnly: return "Diffuse only";
    case LightingDebugMode::SpecularOnly: return "Specular only";
    case LightingDebugMode::Normals: return "World normals";
    case LightingDebugMode::UnlitBaseColor: return "Unlit base color";
    default: return "Unknown";
    }
}

bool validatePhase7Lighting(std::ostream& output)
{
    const SunState morning = SunController::evaluate(SunController::morningTime);
    const SunState noon = SunController::evaluate(SunController::noonTime);
    const SunState evening = SunController::evaluate(SunController::eveningTime);

    bool normalized = true;
    bool statesFinite = true;
    for (const SunState& state : {morning, noon, evening})
    {
        normalized = normalized &&
                     std::abs(glm::length(state.light.direction) - 1.0f) < 1.0e-5f;
        statesFinite = statesFinite && finiteState(state) &&
                       state.light.direction.y < 0.0f && state.light.intensity > 0.0f;
    }
    const bool trajectoryValid = noon.light.direction.y < morning.light.direction.y &&
                                 noon.light.direction.y < evening.light.direction.y &&
                                 glm::distance(morning.light.direction,
                                               evening.light.direction) > 0.5f;

    SunController sixtySteps;
    SunController thirtySteps;
    sixtySteps.setTimeOfDay(SunController::morningTime);
    thirtySteps.setTimeOfDay(SunController::morningTime);
    sixtySteps.setAutomatic(true);
    thirtySteps.setAutomatic(true);
    for (int step = 0; step < 60; ++step)
        sixtySteps.update(1.0f / 60.0f);
    for (int step = 0; step < 30; ++step)
        thirtySteps.update(1.0f / 30.0f);
    const bool frameIndependent = std::abs(sixtySteps.state().timeOfDay -
                                           thirtySteps.state().timeOfDay) < 1.0e-4f;

    SunController looped;
    looped.setTimeOfDay(SunController::daylightEnd - 0.1f);
    looped.setAutomatic(true);
    looped.update(1.0f);
    const bool automaticLoopValid = std::abs(looped.state().timeOfDay -
                                             (SunController::daylightStart + 0.15f)) < 1.0e-4f;

    SunController staticClock;
    staticClock.setTimeOfDay(SunController::noonTime);
    staticClock.setAutomatic(false);
    staticClock.update(120.0f);
    const bool staticClockValid =
        std::abs(staticClock.state().timeOfDay - SunController::noonTime) < 1.0e-6f;

    SunController manual;
    manual.selectMorning();
    const SunState manualMorning = manual.state();
    manual.selectNoon();
    const SunState manualNoon = manual.state();
    manual.selectEvening();
    const SunState manualEvening = manual.state();
    const bool presetsValid = !manual.automatic() &&
                              manualMorning.timeOfDay == SunController::morningTime &&
                              manualNoon.timeOfDay == SunController::noonTime &&
                              manualEvening.timeOfDay == SunController::eveningTime &&
                              glm::all(glm::epsilonEqual(manualMorning.light.direction,
                                                        morning.light.direction, 1.0e-6f)) &&
                              glm::all(glm::epsilonEqual(manualNoon.light.direction,
                                                        noon.light.direction, 1.0e-6f)) &&
                              glm::all(glm::epsilonEqual(manualEvening.light.direction,
                                                        evening.light.direction, 1.0e-6f));

    bool materialsValid = true;
    for (std::size_t index = 0; index < static_cast<std::size_t>(MaterialId::Count); ++index)
    {
        const Material& material = materialDefinition(static_cast<MaterialId>(index));
        materialsValid = materialsValid && finiteVector(material.baseColor) &&
                         glm::all(glm::greaterThanEqual(material.baseColor, glm::vec3{0.0f})) &&
                         glm::all(glm::lessThanEqual(material.baseColor, glm::vec3{1.0f})) &&
                         std::isfinite(material.ambientStrength) &&
                         std::isfinite(material.diffuseStrength) &&
                         std::isfinite(material.specularStrength) &&
                         std::isfinite(material.shininess) &&
                         material.ambientStrength >= 0.0f && material.ambientStrength <= 1.0f &&
                         material.diffuseStrength >= 0.0f && material.diffuseStrength <= 1.5f &&
                         material.specularStrength >= 0.0f && material.specularStrength <= 1.0f &&
                         material.shininess > 0.0f && material.shininess <= 128.0f;
    }

    const glm::mat4 model = makeTransform(
        {3.0f, 2.0f, -4.0f}, {24.0f, 37.0f, 11.0f}, {2.0f, 0.45f, 3.5f});
    const glm::mat3 linear{model};
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(linear));
    const glm::vec3 transformedNormal = glm::normalize(normalMatrix * glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::vec3 transformedTangentX = linear * glm::vec3{1.0f, 0.0f, 0.0f};
    const glm::vec3 transformedTangentZ = linear * glm::vec3{0.0f, 0.0f, 1.0f};
    const bool normalMatrixValid = isFiniteNonSingularTransform(model) &&
                                   finiteVector(transformedNormal) &&
                                   std::abs(glm::length(transformedNormal) - 1.0f) < 1.0e-5f &&
                                   std::abs(glm::dot(transformedNormal, transformedTangentX)) < 1.0e-4f &&
                                   std::abs(glm::dot(transformedNormal, transformedTangentZ)) < 1.0e-4f;

    SunController debug;
    for (int mode = 0; mode < static_cast<int>(LightingDebugMode::Count); ++mode)
        debug.cycleDebugMode();
    const bool debugModesValid = debug.debugMode() == LightingDebugMode::Normal;

    const bool valid = normalized && statesFinite && trajectoryValid && frameIndependent &&
                       automaticLoopValid && staticClockValid && presetsValid &&
                       materialsValid && normalMatrixValid && debugModesValid;
    output << "Phase 7 lighting/material/sun validation\n"
           << "  morning/noon/evening directions normalized: "
           << (normalized ? "PASS\n" : "FAIL\n")
           << "  finite downward daylight states: "
           << (statesFinite ? "PASS\n" : "FAIL\n")
           << "  azimuth/elevation trajectory: "
           << (trajectoryValid ? "PASS\n" : "FAIL\n")
           << "  automatic time frame independence: "
           << (frameIndependent ? "PASS\n" : "FAIL\n")
           << "  automatic daylight loop: "
           << (automaticLoopValid ? "PASS\n" : "FAIL\n")
           << "  static daylight clock remains fixed: "
           << (staticClockValid ? "PASS\n" : "FAIL\n")
           << "  deterministic manual presets: "
           << (presetsValid ? "PASS\n" : "FAIL\n")
           << "  centralized material values: "
           << (materialsValid ? "PASS\n" : "FAIL\n")
           << "  inverse-transpose normal matrix: "
           << (normalMatrixValid ? "PASS\n" : "FAIL\n")
           << "  five debug modes cycle deterministically: "
           << (debugModesValid ? "PASS\n" : "FAIL\n")
           << (valid ? "Phase 7 lighting checks passed.\n"
                     : "Phase 7 lighting checks failed.\n");
    return valid;
}
