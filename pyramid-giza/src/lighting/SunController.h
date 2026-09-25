#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

struct DirectionalLight
{
    // World-space direction in which sunlight rays travel (sun -> scene).
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct SunState
{
    float timeOfDay = 8.0f;
    DirectionalLight light;
    glm::vec3 ambientColor{0.75f, 0.78f, 0.82f};
    float ambientIntensity = 0.85f;
    glm::vec3 skyColor{0.47f, 0.68f, 0.86f};
};

enum class LightingDebugMode
{
    Normal = 0,
    DiffuseOnly,
    SpecularOnly,
    Normals,
    UnlitBaseColor,
    Count
};

class SunController
{
public:
    static constexpr float daylightStart = 6.0f;
    static constexpr float daylightEnd = 18.0f;
    static constexpr float morningTime = 8.0f;
    static constexpr float noonTime = 12.0f;
    static constexpr float eveningTime = 17.0f;
    static constexpr float defaultHoursPerSecond = 0.25f;
    static constexpr float manualStepHours = 0.5f;

    SunController();

    void update(float deltaTime);
    void setTimeOfDay(float hours);
    void adjustTime(float hours);
    void setAutomatic(bool enabled) { automatic_ = enabled; }
    void toggleAutomatic() { automatic_ = !automatic_; }
    bool automatic() const { return automatic_; }
    void setTimeScale(float hoursPerSecond);
    float timeScale() const { return timeScale_; }

    void selectMorning();
    void selectNoon();
    void selectEvening();

    const SunState& state() const { return state_; }

    void cycleDebugMode();
    void setDebugMode(LightingDebugMode mode);
    LightingDebugMode debugMode() const { return debugMode_; }

    static SunState evaluate(float timeOfDay);
    static const char* debugModeName(LightingDebugMode mode);

private:
    SunState state_;
    bool automatic_ = true;
    float timeScale_ = defaultHoursPerSecond;
    LightingDebugMode debugMode_ = LightingDebugMode::Normal;
};

bool validatePhase7Lighting(std::ostream& output);
