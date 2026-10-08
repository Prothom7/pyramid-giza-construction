#include "effects/FireSystem.h"

#include <algorithm>
#include <cmath>
#include <ostream>

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>

#include "scene/SandSimulation.h"

namespace
{
constexpr std::array<glm::vec2, FireSystem::count> sites{{
    {69.0f, -37.0f},   // clear ground by the work camp
    {-88.0f, 27.0f},   // quarry exit / staging edge
    {-49.0f, -141.0f} // dry bank behind the quay
}};

MeshData fireCard()
{
    MeshData data("SharedFireCard");
    data.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

float flicker(double seconds, int index)
{
    const float t = static_cast<float>(seconds);
    const float seed = static_cast<float>(index) * 1.91f;
    return 1.0f + 0.055f * std::sin(2.7f * t + seed) +
           0.035f * std::sin(8.1f * t + seed * 2.3f) +
           0.018f * std::sin(15.7f * t + seed * 3.7f);
}

FireLight lightAt(int index, double seconds, const SunState& sun)
{
    const glm::vec2 xz = sites[static_cast<std::size_t>(index)];
    FireLight light;
    light.position = {xz.x, SandSimulation::staticTerrainHeightAt(xz.x, xz.y) + 0.8f, xz.y};
    light.intensity = 1.8f * std::clamp(sun.nightFactor, 0.0f, 1.0f) *
                      flicker(seconds, index);
    return light;
}

float fract(float x) { return x - std::floor(x); }
bool finite(const glm::vec3& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
} // namespace

FireSystem::FireSystem()
    : shader_("shaders/fire.vert", "shaders/fire.frag"), card_(fireCard())
{
}

void FireSystem::update(float dt)
{
    if (std::isfinite(dt) && dt > 0.0f)
        elapsedSeconds_ += static_cast<double>(dt);
}

void FireSystem::seek(double seconds)
{
    if (std::isfinite(seconds)) elapsedSeconds_ = std::max(0.0, seconds);
}

std::array<glm::vec3, FireSystem::count> FireSystem::locations() const
{
    std::array<glm::vec3, count> result{};
    for (int i = 0; i < count; ++i)
    {
        const glm::vec2 xz = sites[static_cast<std::size_t>(i)];
        result[static_cast<std::size_t>(i)] =
            {xz.x, SandSimulation::staticTerrainHeightAt(xz.x, xz.y), xz.y};
    }
    return result;
}

std::array<FireLight, FireSystem::count> FireSystem::lights(const SunState& sun) const
{
    std::array<FireLight, count> result{};
    for (int i = 0; i < count; ++i)
        result[static_cast<std::size_t>(i)] = lightAt(i, elapsedSeconds_, sun);
    return result;
}

void FireSystem::render(const SunState& sun, const glm::mat4& view,
                        const glm::mat4& projection) const
{
    const float visibility = std::clamp(sun.nightFactor, 0.0f, 1.0f);
    if (visibility < 0.001f) return;

    GLboolean depthWrite = GL_TRUE;
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint srcRgb = 0, dstRgb = 0, srcAlpha = 0, dstAlpha = 0;
    GLint oldProgram = 0, oldVao = 0;
    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
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
    shader_.setVec3("cameraRight", glm::normalize(glm::vec3{view[0][0], view[1][0], view[2][0]}));
    shader_.setFloat("timeSeconds", static_cast<float>(elapsedSeconds_));
    const auto origins = locations();
    for (int i = 0; i < count; ++i)
    {
        const glm::vec3 base = origins[static_cast<std::size_t>(i)];
        const float phase = static_cast<float>(i) * 2.17f;
        shader_.setFloat("phase", phase);
        shader_.setVec3("cardCenter", base + glm::vec3{0.0f, 0.76f, 0.0f});
        shader_.setInt("effectKind", 0);
        shader_.setVec2("cardSize", {1.30f, 1.52f});
        shader_.setFloat("opacity", 0.78f * visibility);
        card_.draw();
        shader_.setInt("effectKind", 1);
        shader_.setVec2("cardSize", {0.88f, 1.18f});
        shader_.setFloat("opacity", 0.84f * visibility);
        card_.draw();

        for (int slot = 0; slot < emberSlotsPerFire; ++slot)
        {
            const float interval = 0.23f;
            const float age = fract(static_cast<float>(elapsedSeconds_) / interval -
                                    static_cast<float>(slot) / emberSlotsPerFire + phase) *
                              interval * emberSlotsPerFire;
            const float side = std::sin(3.4f * static_cast<float>(slot) + phase) * age * 0.18f;
            shader_.setVec3("cardCenter", base +
                glm::vec3{side, 0.58f + age * 1.16f,
                          std::cos(2.8f * slot + phase) * age * 0.12f});
            shader_.setInt("effectKind", 2);
            shader_.setVec2("cardSize", {0.10f, 0.10f});
            shader_.setFloat("opacity", visibility * 0.64f * (1.0f - age / 1.84f));
            card_.draw();
        }
        for (int slot = 0; slot < smokeSlotsPerFire; ++slot)
        {
            const float age = fract(static_cast<float>(elapsedSeconds_) / 1.2f -
                                    static_cast<float>(slot) / smokeSlotsPerFire + phase) * 4.8f;
            shader_.setVec3("cardCenter", base +
                glm::vec3{age * 0.11f, 1.3f + age * 0.55f, age * 0.08f});
            shader_.setInt("effectKind", 3);
            shader_.setVec2("cardSize", {0.55f + age * 0.22f,
                                          0.45f + age * 0.17f});
            shader_.setFloat("opacity", visibility * 0.16f * (1.0f - age / 4.8f));
            card_.draw();
        }
    }
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
    if (!blendEnabled) glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE);
    if (!depthEnabled) glDisable(GL_DEPTH_TEST);
    glDepthMask(depthWrite);
}

bool validateFireSystem(std::ostream& output)
{
    // State/appearance validation is CPU-side and requires no OpenGL context.
    bool valid = true;
    for (const glm::vec2 xz : sites)
    {
        const glm::vec3 p{xz.x, SandSimulation::staticTerrainHeightAt(xz.x, xz.y), xz.y};
        valid = valid && finite(p) && std::abs(p.y) < 20.0f;
    }
    for (const double t : {0.0, 0.5, 7.0, 27.0})
        for (int i = 0; i < FireSystem::count; ++i)
            valid = valid && std::isfinite(flicker(t, i)) &&
                    flicker(t, i) > 0.85f && flicker(t, i) < 1.15f;
    const SunState noon = SunController::evaluate(12.0f);
    const SunState night = SunController::evaluate(21.0f);
    double thirty = 0.0, sixty = 0.0;
    for (int i = 0; i < 300; ++i) thirty += 1.0 / 30.0;
    for (int i = 0; i < 600; ++i) sixty += 1.0 / 60.0;
    float maxEqualTimeError = 0.0f;
    for (int i = 0; i < FireSystem::count; ++i)
    {
        const FireLight day = lightAt(i, 10.0, noon);
        const FireLight dark = lightAt(i, 10.0, night);
        const FireLight repeated = lightAt(i, 10.0, night);
        const FireLight at30 = lightAt(i, thirty, night);
        const FireLight at60 = lightAt(i, sixty, night);
        maxEqualTimeError = std::max(maxEqualTimeError,
            std::abs(at30.intensity - at60.intensity));
        valid = valid && day.intensity == 0.0f &&
                dark.intensity > 1.5f && dark.intensity < 2.1f &&
                dark.position == repeated.position &&
                dark.intensity == repeated.intensity &&
                dark.radius == 12.0f && finite(dark.position);
    }
    valid = valid && maxEqualTimeError < 1.0e-4f;
    output << "Night fire system: " << (valid ? "PASS" : "FAIL")
           << " (3 terrain-grounded sites, 8 ember and 4 smoke slots each; "
           << "30/60 intensity difference " << maxEqualTimeError << ")\n";
    return valid;
}
