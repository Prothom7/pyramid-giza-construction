#pragma once

#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "scene/SceneTypes.h"
#include "graphics/Mesh.h"

struct SphinxBounds
{
    glm::vec3 center{92.0f, 0.0f, -105.0f};
    glm::vec3 minBound{0.0f};
    glm::vec3 maxBound{0.0f};
    float height = 0.0f;
    float length = 0.0f;
    float width = 0.0f;
};

class SphinxMonument
{
public:
    static constexpr glm::vec3 DefaultCenter{92.0f, 0.0f, -105.0f};
    static constexpr float StructuralHalfWidth = 7.25f;
    static constexpr float StructuralFront = -20.0f;
    static constexpr float StructuralRear = 24.0f;
    static constexpr float StructuralHeight = 12.65f;

    static bool containsStructuralFootprint(float x, float z, float margin = 0.0f);
    static MeshData createFormMesh();

    SphinxMonument(const glm::vec3& center = DefaultCenter);

    const glm::vec3& center() const { return center_; }
    SphinxBounds bounds() const;
    std::size_t partCount() const;

    // Generates all procedural components: plinth, lion body, forelegs, paws,
    // shoulders, neck, head, nemes headdress, lappets, uraeus, eyes, nose, mouth,
    // ears, beard, tail, and sand drift accumulation.
    void collectSceneObjects(std::vector<SceneObject>& objects) const;

    static bool validateSphinxMonument(std::ostream& output);

private:
    glm::vec3 center_{DefaultCenter};
};
