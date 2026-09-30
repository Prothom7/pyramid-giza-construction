#include "scene/WaterSimulation.h"

#include <cmath>
#include <iomanip>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>



WaterSimulation::WaterSimulation()
{
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
    updateBoats();
}

void WaterSimulation::update(float deltaTime)
{
    if (!enabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    simulationTime_ += deltaTime;
    updateBoats();
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
    // Shoreline ripple strip along the stone quay (-153.2f)
    for (int section = 0; section < 8; ++section)
    {
        const float x = -42.0f + static_cast<float>(section) * 5.0f;
        const float rippleY = BaseWaterLevel + 0.015f +
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
            const float wakeY = BaseWaterLevel + 0.01f + (1.0f - progress) * 0.025f;

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

    const bool valid = boatsBounded && normalsUpward;

    output << "Phase 13 Water Simulation Validation\n"
           << "  all boats vertically bounded in water surface (+/- 0.35m): "
           << (boatsBounded ? "PASS" : "FAIL") << '\n'
           << "  wave normals normalized and oriented upward: "
           << (normalsUpward ? "PASS" : "FAIL") << '\n'
           << (valid ? "Water simulation validation passed.\n"
                     : "Water simulation validation failed.\n");

    return valid;
}
