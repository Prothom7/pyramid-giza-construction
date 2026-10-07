#include "lighting/SunController.h"

#include <algorithm>
#include <array>
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
    return std::isfinite(state.timeOfDay) &&
           std::isfinite(state.solarElevationDegrees) &&
           std::isfinite(state.daylightFactor) &&
           std::isfinite(state.nightFactor) &&
           finiteVector(state.light.direction) &&
           finiteVector(state.light.color) && std::isfinite(state.light.intensity) &&
           finiteVector(state.ambientColor) && std::isfinite(state.ambientIntensity) &&
           finiteVector(state.skyColor);
}

double wrapDay(double hours)
{
    double wrapped = std::fmod(hours, static_cast<double>(SunController::dayEnd));
    if (wrapped < 0.0f)
        wrapped += static_cast<double>(SunController::dayEnd);
    return wrapped;
}

float smoothstep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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
    const float time = std::clamp(timeOfDay, dayStart, dayEnd);
    const float daylightTime = std::clamp(time, daylightStart, daylightEnd);
    const float progress = (daylightTime - daylightStart) /
        (daylightEnd - daylightStart);
    const float solarArc = std::sin(glm::pi<float>() * progress);
    // The established 06:00–18:00 arc is unchanged. Below the horizon, the
    // same solar direction continues through midnight and meets dawn again.
    const float nightHours = time >= daylightEnd ? time - daylightEnd : time + 6.0f;
    const float elevationDegrees = time >= daylightStart && time <= daylightEnd
        ? 12.0f + 66.0f * solarArc
        : 12.0f - 67.0f * std::sin(glm::pi<float>() * nightHours / 12.0f);
    const float azimuthDegrees = time >= daylightStart && time <= daylightEnd
        ? -105.0f + 210.0f * progress
        : (time > daylightEnd ? 105.0f + 12.5f * (time - daylightEnd)
                              : -180.0f + 12.5f * time);
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
    state.solarElevationDegrees = elevationDegrees;
    // 04:30–06:00 and 18:00–19:30 remain twilight. The accepted daylight
    // states inside 06:00–18:00 retain their original exact palette and power.
    state.daylightFactor = time < daylightStart
        ? smoothstep((time - 4.5f) / 1.5f)
        : (time <= daylightEnd ? 1.0f :
            1.0f - smoothstep((time - daylightEnd) / 1.5f));
    state.nightFactor = 1.0f - state.daylightFactor;
    state.light.direction = -glm::normalize(toSun);
    const glm::vec3 daylightLight = daylightBlend(
        {1.00f, 0.78f, 0.60f}, {1.00f, 0.96f, 0.86f},
        {1.00f, 0.70f, 0.52f}, progress);
    state.light.color = glm::mix(glm::vec3{0.26f, 0.31f, 0.43f},
                                 daylightLight, state.daylightFactor);
    state.light.intensity = (0.72f + 0.38f * solarArc) * state.daylightFactor;
    const glm::vec3 daylightAmbient = daylightBlend(
        {0.78f, 0.74f, 0.70f}, {0.72f, 0.82f, 1.00f},
        {0.76f, 0.69f, 0.68f}, progress);
    state.ambientColor = glm::mix(glm::vec3{0.45f, 0.48f, 0.54f},
                                  daylightAmbient, state.daylightFactor);
    state.ambientIntensity = glm::mix(0.62f, 0.78f + 0.12f * solarArc,
                                      state.daylightFactor);
    const glm::vec3 daylightSky = daylightBlend(
        {0.62f, 0.67f, 0.76f}, {0.45f, 0.68f, 0.90f},
        {0.72f, 0.56f, 0.48f}, progress);
    state.skyColor = glm::mix(glm::vec3{0.035f, 0.055f, 0.105f},
                              daylightSky, state.daylightFactor);
    return state;
}

void SunController::update(float deltaTime)
{
    if (!automatic_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;
    preciseTimeHours_ = wrapDay(preciseTimeHours_ +
        static_cast<double>(timeScale_) * static_cast<double>(deltaTime));
    state_ = evaluate(static_cast<float>(preciseTimeHours_));
}

void SunController::setTimeOfDay(float hours)
{
    if (!std::isfinite(hours))
        return;
    preciseTimeHours_ = std::clamp(static_cast<double>(hours),
                                   static_cast<double>(dayStart),
                                   static_cast<double>(dayEnd));
    state_ = evaluate(static_cast<float>(preciseTimeHours_));
}

void SunController::adjustTime(float hours)
{
    if (!std::isfinite(hours))
        return;
    automatic_ = false;
    preciseTimeHours_ = wrapDay(preciseTimeHours_ + static_cast<double>(hours));
    state_ = evaluate(static_cast<float>(preciseTimeHours_));
}

void SunController::setTimeScale(float hoursPerSecond)
{
    if (std::isfinite(hoursPerSecond) && hoursPerSecond > 0.0f)
        timeScale_ = hoursPerSecond;
}

void SunController::selectMorning()
{
    automatic_ = false;
    setTimeOfDay(morningTime);
}

void SunController::selectNoon()
{
    automatic_ = false;
    setTimeOfDay(noonTime);
}

void SunController::selectEvening()
{
    automatic_ = false;
    setTimeOfDay(eveningTime);
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
    looped.setTimeOfDay(SunController::dayEnd - 0.1f);
    looped.setAutomatic(true);
    looped.update(1.0f);
    const bool automaticLoopValid = std::abs(looped.state().timeOfDay -
                                             0.15f) < 1.0e-4f;

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
           << "  automatic 24-hour loop: "
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

bool validate24HourEnvironment(std::ostream& output)
{
    constexpr std::array<float, 8> hours{0.0f, 5.0f, 8.0f, 12.0f,
                                         17.0f, 19.0f, 21.0f, 23.0f};
    bool finite = true;
    for (const float hour : hours)
    {
        const SunState state = SunController::evaluate(hour);
        finite = finite && finiteState(state) &&
            std::abs(glm::length(state.light.direction) - 1.0f) < 1.0e-5f &&
            state.daylightFactor >= 0.0f && state.daylightFactor <= 1.0f &&
            state.nightFactor >= 0.0f && state.nightFactor <= 1.0f &&
            std::abs(state.daylightFactor + state.nightFactor - 1.0f) < 1.0e-6f;
    }
    bool daylightPreserved = true;
    for (const float hour : {8.0f, 12.0f, 17.0f})
    {
        const float progress = (hour - SunController::daylightStart) /
            (SunController::daylightEnd - SunController::daylightStart);
        const float arc = std::sin(glm::pi<float>() * progress);
        const SunState state = SunController::evaluate(hour);
        daylightPreserved = daylightPreserved &&
            state.daylightFactor == 1.0f && state.nightFactor == 0.0f &&
            std::abs(state.solarElevationDegrees - (12.0f + 66.0f * arc)) < 1.0e-5f &&
            std::abs(state.light.intensity - (0.72f + 0.38f * arc)) < 1.0e-5f;
    }
    const SunState midnight = SunController::evaluate(0.0f);
    const SunState evening = SunController::evaluate(19.0f);
    const SunState late = SunController::evaluate(21.0f);
    const SunState end = SunController::evaluate(24.0f);
    const bool night = midnight.solarElevationDegrees < -40.0f &&
        midnight.light.direction.y > 0.0f &&
        evening.solarElevationDegrees < 0.0f &&
        late.light.intensity == 0.0f && midnight.light.intensity == 0.0f &&
        midnight.ambientIntensity > 0.25f &&
        midnight.skyColor.b < SunController::evaluate(12.0f).skyColor.b * 0.2f;
    const bool loop = glm::distance(midnight.light.direction,
                                    end.light.direction) < 1.0e-5f &&
        midnight.skyColor == end.skyColor;
    bool continuous = true;
    for (const float boundary : {6.0f, 18.0f, 24.0f})
    {
        const SunState before = SunController::evaluate(boundary - 0.001f);
        const SunState after = SunController::evaluate(boundary == 24.0f
            ? 0.001f : boundary + 0.001f);
        continuous = continuous &&
            glm::distance(before.light.direction, after.light.direction) < 0.01f &&
            glm::distance(before.skyColor, after.skyColor) < 0.01f &&
            std::abs(before.light.intensity - after.light.intensity) < 0.01f;
    }
    SunController thirty, sixty;
    thirty.setTimeOfDay(23.9f);
    sixty.setTimeOfDay(23.9f);
    for (int i = 0; i < 30; ++i) thirty.update(1.0f / 30.0f);
    for (int i = 0; i < 60; ++i) sixty.update(1.0f / 60.0f);
    const bool frameIndependent =
        std::abs(thirty.state().timeOfDay - sixty.state().timeOfDay) < 1.0e-4f &&
        thirty.state().timeOfDay < 1.0f;
    SunController longThirty, longSixty;
    longThirty.setTimeOfDay(0.0f);
    longSixty.setTimeOfDay(0.0f);
    for (int i = 0; i < 3000; ++i) longThirty.update(1.0f / 30.0f);
    for (int i = 0; i < 6000; ++i) longSixty.update(1.0f / 60.0f);
    const bool longRun = std::abs(longThirty.state().timeOfDay -
                                  longSixty.state().timeOfDay) < 1.0e-4f &&
                         std::abs(longThirty.state().timeOfDay - 1.0f) < 1.0e-4f;
    SunController manual;
    manual.setTimeOfDay(0.25f);
    manual.adjustTime(-0.5f);
    const bool manualWrap = !manual.automatic() &&
        std::abs(manual.state().timeOfDay - 23.75f) < 1.0e-5f;
    const bool valid = finite && daylightPreserved && night && loop &&
        continuous && frameIndependent && longRun && manualWrap;
    output << "Full 24-hour SunController validation\n"
           << "  finite representative times and normalized orbit: "
           << (finite ? "PASS" : "FAIL") << '\n'
           << "  08:00 / 12:00 / 17:00 daylight preserved: "
           << (daylightPreserved ? "PASS" : "FAIL") << '\n'
           << "  below-horizon night, dim sky, nonzero ambient: "
           << (night ? "PASS" : "FAIL") << '\n'
           << "  dawn/dusk/midnight continuity and 00:00/24:00 agreement: "
           << (continuous && loop ? "PASS" : "FAIL") << '\n'
           << "  30/60 FPS, full-day wrap, and manual midnight wrapping: "
           << (frameIndependent && longRun && manualWrap ? "PASS" : "FAIL") << '\n'
           << (valid ? "24-hour environment validation passed.\n"
                     : "24-hour environment validation failed.\n");
    return valid;
}
