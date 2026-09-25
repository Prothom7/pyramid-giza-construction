#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

struct SceneBounds
{
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
};

class SceneIntegrity
{
public:
    static SceneBounds contentBounds();
    static SceneBounds finalGroundBounds();
    static glm::vec4 horizontalSafetyMargins();
};

bool validateSceneIntegrity(std::ostream& output);
