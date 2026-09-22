#include "graphics/ShowcaseScene.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics/PrimitiveGenerator.h"

namespace
{
glm::mat4 makeModel(const glm::vec3& translation, const glm::vec3& rotationDegrees,
                    const glm::vec3& scale)
{
    glm::mat4 model{1.0f};
    model = glm::translate(model, translation);
    model = glm::rotate(model, glm::radians(rotationDegrees.y), {0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, glm::radians(rotationDegrees.x), {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(rotationDegrees.z), {0.0f, 0.0f, 1.0f});
    return glm::scale(model, scale); // GLM post-multiplies, producing T * R * S.
}
} // namespace

ShowcaseScene::ShowcaseScene()
    : shader_("shaders/basic.vert", "shaders/basic.frag"),
      triangle_(PrimitiveGenerator::createTriangle()),
      plane_(PrimitiveGenerator::createPlane()),
      cube_(PrimitiveGenerator::createCube()),
      pyramid_(PrimitiveGenerator::createPyramid()),
      disk_(PrimitiveGenerator::createDisk()),
      cylinder_(PrimitiveGenerator::createCylinder()),
      sphere_(PrimitiveGenerator::createSphere())
{
}

void ShowcaseScene::draw(const Mesh& mesh, const glm::mat4& model, const glm::vec3& color)
{
    shader_.setMat4("model", model);
    shader_.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    shader_.setVec3("objectColor", color);
    mesh.draw();
}

void ShowcaseScene::render(const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec3& cameraPosition, float elapsedSeconds)
{
    shader_.use();
    shader_.setMat4("view", view);
    shader_.setMat4("projection", projection);
    shader_.setVec3("viewPosition", cameraPosition);
    shader_.setVec3("lightDirection", {-0.45f, -1.0f, -0.35f});
    shader_.setVec3("lightColor", {1.0f, 0.97f, 0.90f});

    const float turn = elapsedSeconds * 12.0f;
    draw(triangle_, makeModel({-3.6f, 1.35f, 0.0f}, {5.0f, -15.0f, 0.0f}, {1.35f, 1.35f, 1.35f}),
         {0.92f, 0.35f, 0.25f});
    draw(plane_, makeModel({-1.25f, 1.35f, 0.0f}, {62.0f, 18.0f, 0.0f}, {1.65f, 1.65f, 1.65f}),
         {0.28f, 0.72f, 0.45f});

    // All three blocks share one cube VAO/VBO/EBO. The second uses non-uniform
    // scale, visibly exercising the inverse-transpose normal matrix.
    draw(cube_, makeModel({1.15f, 1.45f, 0.0f}, {20.0f, 35.0f + turn, 0.0f}, {1.15f, 1.15f, 1.15f}),
         {0.25f, 0.52f, 0.92f});
    draw(cube_, makeModel({0.55f, 0.45f, 0.1f}, {10.0f, -25.0f, 5.0f}, {1.35f, 0.35f, 0.55f}),
         {0.38f, 0.65f, 0.95f});
    draw(cube_, makeModel({1.85f, 0.45f, 0.1f}, {-8.0f, 20.0f, 0.0f}, {0.48f, 0.78f, 0.48f}),
         {0.18f, 0.42f, 0.82f});

    draw(pyramid_, makeModel({3.6f, 1.3f, 0.0f}, {0.0f, -28.0f + turn, 0.0f}, {1.5f, 1.5f, 1.5f}),
         {0.90f, 0.67f, 0.27f});
    draw(disk_, makeModel({-2.45f, -1.05f, 0.0f}, {65.0f, -18.0f, 0.0f}, {1.55f, 1.55f, 1.55f}),
         {0.78f, 0.30f, 0.72f});
    draw(cylinder_, makeModel({0.0f, -1.0f, 0.0f}, {12.0f, 25.0f + turn, 0.0f}, {1.35f, 1.65f, 1.35f}),
         {0.20f, 0.72f, 0.78f});
    draw(sphere_, makeModel({2.45f, -1.0f, 0.0f}, {0.0f, turn, 0.0f}, {1.55f, 1.55f, 1.55f}),
         {0.88f, 0.42f, 0.18f});
}
