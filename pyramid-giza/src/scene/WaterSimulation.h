#pragma once

#include <cstddef>
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
    glm::vec3 wakeOrigin{0.0f};
    bool moored = false;
    float centerWaterY = 0.0f;
    float bowWaterY = 0.0f;
    float sternWaterY = 0.0f;
    float portWaterY = 0.0f;
    float starboardWaterY = 0.0f;
};

class WaterSimulation
{
public:
    static constexpr float DominantFlowSpeed = 0.85f; // meters / second

    WaterSimulation();

    void update(float deltaTime);
    void reset();

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void toggleEnabled() { enabled_ = !enabled_; }

    float waveHeightAt(float x, float z) const;
    glm::vec3 waveNormalAt(float x, float z) const;
    glm::vec2 flowVelocityAt(float x, float z) const;
    bool waterSurfaceAt(float x, float z, float& height) const;
    const std::vector<Vertex>& surfaceVertices() const { return surfaceVertices_; }

    const std::vector<SimulatedBoatState>& boatStates() const { return boatStates_; }
    glm::mat4 boatRootTransform(std::size_t index) const;

    // Visual geometry generation (boat dynamic transforms, wake ripples, shoreline foam strips)
    void collectSceneObjects(std::vector<SceneObject>& objects) const;

    static bool validateWaterSimulation(std::ostream& output);
    static bool validateBoatWaterCoupling(std::ostream& output);

private:
    void initBoats();
    void updateBoats();
    void updateSurface();
    float attenuatedWaveHeightAt(float x, float z) const;

    float simulationTime_ = 0.0f;
    bool enabled_ = true;
    std::vector<SimulatedBoatState> boatStates_;
    std::vector<BoatDescriptor> baseBoats_;
    MeshData baseSurfaceMesh_{"NileSurface"};
    std::vector<Vertex> surfaceVertices_;
    std::vector<glm::vec2> surfaceWorldXZ_;
};
