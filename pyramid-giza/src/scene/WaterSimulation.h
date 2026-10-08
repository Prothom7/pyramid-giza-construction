#pragma once

#include <cstddef>
#include <array>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "graphics/Mesh.h"
#include "scene/ObjectEnrichment.h"
#include "scene/SceneTypes.h"

struct SimulatedBoatState
{
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    float verticalDisplacement = 0.0f;
    float pitchDegrees = 0.0f;
    float rollDegrees = 0.0f;
    float swayDegrees = 0.0f;
    glm::vec2 horizontalVelocity{0.0f}; // world X/Z, navigation only
    bool moored = false;
    float centerWaterY = 0.0f;
    float bowWaterY = 0.0f;
    float sternWaterY = 0.0f;
    float portWaterY = 0.0f;
    float starboardWaterY = 0.0f;
};

struct PropulsionWake
{
    glm::vec2 position{0.0f}; // Fixed world X/Z after emission.
    glm::vec2 direction{0.0f, 1.0f};
    float birthTime = 0.0f;
    float emissionDistance = 0.0f;
    bool active = false;
};

class WaterSimulation
{
public:
    static constexpr float DominantFlowSpeed = 0.85f; // meters / second

    WaterSimulation();

    void update(float deltaTime);
    void reset();
    void setSupplyBoatManual(bool manual);
    bool supplyBoatManual() const { return supplyBoatManual_; }
    void setManualBoatInput(float throttle, float turn);

    bool enabled() const { return enabled_; }
    float timeSeconds() const { return simulationTime_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void toggleEnabled() { enabled_ = !enabled_; }

    float waveHeightAt(float x, float z) const;
    glm::vec3 waveNormalAt(float x, float z) const;
    glm::vec2 flowVelocityAt(float x, float z) const;
    bool waterSurfaceAt(float x, float z, float& height) const;
    const std::vector<Vertex>& surfaceVertices() const { return surfaceVertices_; }

    const std::vector<SimulatedBoatState>& boatStates() const { return boatStates_; }
    glm::mat4 boatRootTransform(std::size_t index) const;
    const std::array<PropulsionWake, 16>& propulsionWakes() const { return wakes_; }
    std::size_t propulsionEmissionCount() const { return wakeEmissionCount_; }

    // Shared-mesh quay ripples and emitted supply-boat propulsion wakes.
    void collectSceneObjects(std::vector<SceneObject>& objects) const;

    static bool validateWaterSimulation(std::ostream& output);
    static bool validateWaterAppearance(std::ostream& output);
    static bool validateBoatWaterCoupling(std::ostream& output);
    static bool validateBoatNavigation(std::ostream& output);
    static bool validateBoatWake(std::ostream& output);
    static bool validateManualBoat(std::ostream& output);

private:
    void initBoats();
    void updateBoats();
    void updateSurface();
    void updatePropulsionWake(float previousDistance, float currentDistance,
                              float previousTime);
    void updateManualWake(float previousDistance, float previousTime,
                          const glm::vec2& previousPosition, float previousYaw);
    bool manualFootprintInside(const BoatDescriptor& boat,
                               const glm::vec2& position, float yawDegrees) const;
    float attenuatedWaveHeightAt(float x, float z) const;

    float simulationTime_ = 0.0f;
    bool enabled_ = true;
    std::vector<SimulatedBoatState> boatStates_;
    std::vector<BoatDescriptor> baseBoats_;
    std::array<PropulsionWake, 16> wakes_{};
    float nextWakeDistance_ = 1.5f;
    std::size_t wakeEmissionCount_ = 0;
    bool supplyBoatManual_ = false;
    glm::vec2 manualPosition_{0.0f};
    glm::vec2 manualVelocity_{0.0f};
    float manualYawDegrees_ = 0.0f;
    float manualThrottle_ = 0.0f;
    float manualTurn_ = 0.0f;
    float manualDistance_ = 0.0f;
    float automaticRouteTime_ = 0.0f;
    MeshData baseSurfaceMesh_{"NileSurface"};
    std::vector<Vertex> surfaceVertices_;
    std::vector<glm::vec2> surfaceWorldXZ_;
};
