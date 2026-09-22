#pragma once

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"

class ShowcaseScene
{
public:
    ShowcaseScene();

    void render(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPosition, float elapsedSeconds);

private:
    void draw(const Mesh& mesh, const glm::mat4& model, const glm::vec3& color);

    Shader shader_;
    Mesh triangle_;
    Mesh plane_;
    Mesh cube_;
    Mesh pyramid_;
    Mesh disk_;
    Mesh cylinder_;
    Mesh sphere_;
};
