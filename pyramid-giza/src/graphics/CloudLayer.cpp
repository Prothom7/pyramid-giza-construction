#include "graphics/CloudLayer.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ostream>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr std::array<CloudDescriptor, CloudField::count> initialClouds{{
    {{-540.0f, 184.0f, -470.0f}, {210.0f, 67.0f}, 0.72f, 0.35f, 0.13f},
    {{-220.0f, 178.0f, -740.0f}, {250.0f, 72.0f}, 0.83f, 0.32f, 0.42f},
    {{ 200.0f, 191.0f, -770.0f}, {225.0f, 64.0f}, 0.67f, 0.33f, 0.77f},
    {{ 660.0f, 180.0f, -420.0f}, {235.0f, 70.0f}, 0.98f, 0.34f, 0.25f},
    {{ 720.0f, 188.0f,  180.0f}, {205.0f, 62.0f}, 1.05f, 0.31f, 0.61f},
    {{ 430.0f, 176.0f,  650.0f}, {240.0f, 74.0f}, 0.76f, 0.35f, 0.91f},
    {{-190.0f, 182.0f,  730.0f}, {215.0f, 68.0f}, 0.89f, 0.32f, 0.36f},
    {{-700.0f, 180.0f,  320.0f}, {230.0f, 70.0f}, 0.64f, 0.34f, 0.84f}
}};

MeshData cloudCard()
{
    MeshData data("SharedCloudCard");
    data.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

float edgeOpacity(float x)
{
    const float edge = std::min(x - static_cast<float>(CloudField::westEdge),
                                static_cast<float>(CloudField::eastEdge) - x);
    const float t = std::clamp(edge / 190.0f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool finite(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
} // namespace

void CloudField::update(float dt)
{
    if (std::isfinite(dt) && dt > 0.0f)
        elapsedSeconds_ += static_cast<double>(dt);
}

void CloudField::seek(double seconds)
{
    if (std::isfinite(seconds))
        elapsedSeconds_ = std::max(0.0, seconds);
}

std::array<CloudDescriptor, CloudField::count> CloudField::clouds() const
{
    auto result = initialClouds;
    constexpr double width = eastEdge - westEdge;
    for (CloudDescriptor& cloud : result)
    {
        double x = std::fmod(static_cast<double>(cloud.center.x) - westEdge +
                             static_cast<double>(cloud.speed) * elapsedSeconds_, width);
        if (x < 0.0) x += width;
        cloud.center.x = static_cast<float>(westEdge + x);
        cloud.opacity *= edgeOpacity(cloud.center.x);
    }
    return result;
}

glm::vec3 CloudField::tint(const SunState& sun)
{
    // Existing sunlight and ambient supply the only daylight palette.
    const glm::vec3 lit = 0.66f * sun.light.color +
                          0.34f * sun.ambientColor;
    return glm::clamp(glm::mix(glm::vec3{0.86f, 0.87f, 0.86f}, lit,
                               0.36f * std::clamp(sun.light.intensity, 0.0f, 1.0f)),
                      0.0f, 0.96f);
}

CloudLayer::CloudLayer()
    : shader_("shaders/cloud.vert", "shaders/cloud.frag"), card_(cloudCard())
{
}

void CloudLayer::render(const SunState& sun, const glm::mat4& view,
                        const glm::mat4& projection,
                        const glm::vec3& cameraPosition) const
{
    const auto clouds = field_.clouds();
    std::array<std::size_t, CloudField::count> order{};
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const glm::vec3 da = clouds[a].center - cameraPosition;
        const glm::vec3 db = clouds[b].center - cameraPosition;
        return glm::dot(da, da) > glm::dot(db, db);
    });

    const GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasBlend = glIsEnabled(GL_BLEND);
    const GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
    GLboolean depthWrite = GL_TRUE;
    GLint blendSrcRgb = 0, blendDstRgb = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
    GLint blendEqRgb = 0, blendEqAlpha = 0, previousProgram = 0, previousVao = 0;
    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqAlpha);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_POLYGON_MODE, polygonMode);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader_.use();
    shader_.setMat4("view", view);
    shader_.setMat4("projection", projection);
    const glm::mat3 cameraToWorld = glm::transpose(glm::mat3(view));
    shader_.setVec3("cameraRight", cameraToWorld[0]);
    shader_.setVec3("cameraUp", cameraToWorld[1]);
    shader_.setVec3("cloudTint", CloudField::tint(sun));
    for (const std::size_t index : order)
    {
        const CloudDescriptor& cloud = clouds[index];
        if (cloud.opacity <= 0.0f) continue;
        shader_.setVec3("cloudCenter", cloud.center);
        shader_.setVec2("cloudSize", cloud.size);
        shader_.setFloat("cloudOpacity", cloud.opacity);
        shader_.setFloat("shapeSeed", cloud.seed);
        card_.draw();
    }

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glBlendEquationSeparate(blendEqRgb, blendEqAlpha);
    glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
    if (wasCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (wasBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(depthWrite);
}

bool validateCloudLayer(std::ostream& output)
{
    CloudField field;
    const auto initial = field.clouds();
    const MeshData card = cloudCard();
    bool valid = card.vertices.size() == 4 && card.indices.size() == 6;
    float minBottom = 1.0e6f, minSpeed = 1.0e6f, maxSpeed = 0.0f;
    for (const CloudDescriptor& c : initial)
    {
        minBottom = std::min(minBottom, c.center.y - 0.5f * c.size.y);
        minSpeed = std::min(minSpeed, c.speed);
        maxSpeed = std::max(maxSpeed, c.speed);
        valid = valid && finite(c.center) && c.size.x > 0.0f && c.size.y > 0.0f &&
                std::isfinite(c.opacity) && c.opacity > 0.0f && c.opacity <= 1.0f &&
                c.center.x >= CloudField::westEdge && c.center.x <= CloudField::eastEdge;
    }
    valid = valid && minBottom > 100.0f && initial.size() == CloudField::count;
    CloudField second;
    valid = valid && second.clouds()[0].center == initial[0].center;
    for (int i = 0; i < 300; ++i) field.update(1.0f / 30.0f);
    for (int i = 0; i < 600; ++i) second.update(1.0f / 60.0f);
    float equalTimeError = 0.0f;
    const auto a = field.clouds(), b = second.clouds();
    for (std::size_t i = 0; i < CloudField::count; ++i)
        equalTimeError = std::max(equalTimeError, glm::distance(a[i].center, b[i].center));
    valid = valid && equalTimeError < 1.0e-4f;
    field.reset();
    valid = valid && field.clouds()[0].center == initial[0].center;
    field.seek(900.0);
    const auto wrapped = field.clouds();
    valid = valid && wrapped.size() == CloudField::count &&
            wrapped[4].center.x < initial[4].center.x;
    for (const CloudDescriptor& c : wrapped)
        valid = valid && finite(c.center) && c.center.x >= CloudField::westEdge &&
                c.center.x <= CloudField::eastEdge;
    field.seek(900.0);
    valid = valid && field.clouds()[4].center == wrapped[4].center;
    const glm::vec3 morning = CloudField::tint(SunController::evaluate(8.0f));
    const glm::vec3 noon = CloudField::tint(SunController::evaluate(12.0f));
    const glm::vec3 evening = CloudField::tint(SunController::evaluate(17.0f));
    valid = valid && finite(morning) && finite(noon) && finite(evening) &&
            glm::all(glm::lessThanEqual(morning, glm::vec3{1.0f})) &&
            glm::all(glm::lessThanEqual(noon, glm::vec3{1.0f})) &&
            glm::all(glm::lessThanEqual(evening, glm::vec3{1.0f})) &&
            glm::distance(morning, noon) > 0.005f &&
            glm::distance(noon, evening) > 0.005f;
    output << "Deterministic cloud validation: " << (valid ? "PASS" : "FAIL")
           << "\n  count=" << CloudField::count << " indexed triangles=" << card.indices.size()/3
           << " minimum bottom=" << minBottom << " speed=" << minSpeed << ".." << maxSpeed
           << " equal-time position error=" << equalTimeError << '\n';
    return valid;
}
