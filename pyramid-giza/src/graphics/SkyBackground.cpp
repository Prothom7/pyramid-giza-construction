#include "graphics/SkyBackground.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <ostream>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera/CameraController.h"

namespace
{
constexpr float ZenithElevationScale = 0.35f;

MeshData skyQuad()
{
    MeshData data("SkyBackgroundQuad");
    data.vertices = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

bool finiteColor(const glm::vec3& color)
{
    return std::isfinite(color.r) && std::isfinite(color.g) &&
           std::isfinite(color.b) &&
           color.r >= 0.0f && color.g >= 0.0f && color.b >= 0.0f &&
           color.r <= 1.0f && color.g <= 1.0f && color.b <= 1.0f;
}

float smoothElevation(float elevation)
{
    const float t = std::clamp(elevation / ZenithElevationScale, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

SkyColors skyColors(const SunState& sun)
{
    // A low sun warms the horizon; the existing sky color retains each preset's
    // character. This derives presentation colors without a second daylight clock.
    const float lowSun = 1.0f - std::clamp(-sun.light.direction.y, 0.0f, 1.0f);
    const glm::vec3 horizonTint =
        0.55f * sun.ambientColor + 0.45f * sun.light.color;
    return {
        glm::clamp(glm::mix(sun.skyColor, horizonTint,
                            0.45f + 0.10f * lowSun), 0.0f, 1.0f),
        glm::clamp(sun.skyColor * glm::vec3{0.86f, 0.93f, 1.05f}, 0.0f, 1.0f)};
}

glm::vec3 skyViewDirection(const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec2& ndc)
{
    // For an orthonormal camera view, the transpose of its rotation is the
    // camera-to-world rotation. Translation never enters this calculation.
    const glm::mat3 cameraToWorld = glm::transpose(glm::mat3(view));
    const glm::vec3 cameraRay{ndc.x / projection[0][0],
                              ndc.y / projection[1][1], -1.0f};
    return glm::normalize(cameraToWorld * cameraRay);
}

glm::vec3 skyColorAt(const SkyColors& colors, const glm::vec3& worldDirection)
{
    return glm::mix(colors.horizon, colors.zenith,
                    smoothElevation(worldDirection.y));
}

SkyBackground::SkyBackground()
    : shader_("shaders/sky.vert", "shaders/sky.frag"), quad_(skyQuad())
{
}

void SkyBackground::render(const SunState& sun, const glm::mat4& view,
                           const glm::mat4& projection) const
{
    const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cull = glIsEnabled(GL_CULL_FACE);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean depthWrite = GL_TRUE;
    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    GLint previousProgram = 0;
    GLint previousVao = 0;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    glGetIntegerv(GL_POLYGON_MODE, polygonMode);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader_.use();
    const SkyColors colors = skyColors(sun);
    shader_.setVec3("horizonColor", colors.horizon);
    shader_.setVec3("zenithColor", colors.zenith);
    shader_.setFloat("zenithElevationScale", ZenithElevationScale);
    shader_.setMat3("cameraToWorld", glm::transpose(glm::mat3(view)));
    shader_.setVec2("projectionScale",
                    {1.0f / projection[0][0], 1.0f / projection[1][1]});
    quad_.draw(); // shared Mesh uses glDrawElements(GL_TRIANGLES).

    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(depthWrite);
}

bool validateSkyBackground(std::ostream& output)
{
    const MeshData mesh = skyQuad();
    const bool indexedQuad = mesh.vertices.size() == 4 && mesh.indices.size() == 6 &&
                             mesh.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3};
    const SunState morning = SunController::evaluate(8.0f);
    const SunState noon = SunController::evaluate(12.0f);
    const SunState evening = SunController::evaluate(17.0f);
    const SkyColors m = skyColors(morning);
    const SkyColors n = skyColors(noon);
    const SkyColors e = skyColors(evening);
    const bool finite = finiteColor(m.horizon) && finiteColor(m.zenith) &&
                        finiteColor(n.horizon) && finiteColor(n.zenith) &&
                        finiteColor(e.horizon) && finiteColor(e.zenith);
    const bool deterministic = skyColors(morning).horizon == m.horizon &&
                               skyColors(morning).zenith == m.zenith;
    const bool daylight = glm::distance(m.horizon, n.horizon) > 0.02f &&
                          glm::distance(n.horizon, e.horizon) > 0.02f &&
                          glm::distance(m.zenith, e.zenith) > 0.02f;
    const glm::vec3 horizontal{1.0f, 0.0f, 0.0f};
    const bool worldHorizon = glm::distance(skyColorAt(n, horizontal), n.horizon) < 1e-6f &&
                              glm::distance(skyColorAt(n, {0.0f, 1.0f, 0.0f}), n.zenith) < 1e-6f &&
                              glm::distance(skyColorAt(n, {0.0f, -0.2f, -0.98f}), n.horizon) < 1e-6f;
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f,
                                                  0.7f, 700.0f);
    const glm::vec3 forward{0.0f, 0.0f, -1.0f};
    const glm::mat4 first = glm::lookAt(glm::vec3{0.0f, 2.0f, 0.0f},
                                        glm::vec3{0.0f, 2.0f, 0.0f} + forward,
                                        glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 moved = glm::lookAt(glm::vec3{100.0f, 40.0f, -50.0f},
                                        glm::vec3{100.0f, 40.0f, -50.0f} + forward,
                                        glm::vec3{0.0f, 1.0f, 0.0f});
    const bool translation = glm::distance(skyViewDirection(first, projection, {0.2f, 0.3f}),
                                           skyViewDirection(moved, projection, {0.2f, 0.3f})) < 1e-5f;
    const glm::mat4 up = glm::lookAt(glm::vec3{0.0f}, glm::vec3{0.0f, 0.8f, -0.6f},
                                     glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 down = glm::lookAt(glm::vec3{0.0f}, glm::vec3{0.0f, -0.5f, -0.866f},
                                       glm::vec3{0.0f, 1.0f, 0.0f});
    const bool rotation = skyViewDirection(up, projection, {0.0f, 0.0f}).y > 0.7f &&
                          skyViewDirection(down, projection, {0.0f, 0.0f}).y < -0.4f &&
                          skyColorAt(n, skyViewDirection(up, projection, {0.0f, 0.0f})).b >
                          skyColorAt(n, skyViewDirection(down, projection, {0.0f, 0.0f})).b;
    const glm::mat4 wide = glm::perspective(glm::radians(60.0f), 21.0f / 9.0f,
                                            0.7f, 700.0f);
    const bool aspect = skyViewDirection(first, wide, {1.0f, 0.0f}).x >
                        skyViewDirection(first, projection, {1.0f, 0.0f}).x;
    bool presets = true;
    for (const CameraPose& pose : CameraController::presets())
    {
        const Camera camera(pose.position, {0.0f, 1.0f, 0.0f},
                            pose.yaw, pose.pitch);
        const glm::mat4 presetProjection = glm::perspective(
            glm::radians(pose.fovDegrees), 16.0f / 9.0f, 0.7f, 700.0f);
        for (const glm::vec2 ndc : {glm::vec2{-1.0f, -1.0f},
                                    glm::vec2{1.0f, -1.0f},
                                    glm::vec2{0.0f, 0.0f},
                                    glm::vec2{-1.0f, 1.0f},
                                    glm::vec2{1.0f, 1.0f}})
        {
            const glm::vec3 ray = skyViewDirection(camera.GetViewMatrix(),
                                                    presetProjection, ndc);
            presets = presets && std::isfinite(ray.x) && std::isfinite(ray.y) &&
                      std::isfinite(ray.z) &&
                      std::abs(glm::length(ray) - 1.0f) < 1e-5f &&
                      finiteColor(skyColorAt(n, ray));
        }
    }
    const bool valid = indexedQuad && finite && deterministic && daylight &&
                       worldHorizon && translation && rotation && aspect && presets;
    output << "Sun-driven sky-background validation\n"
           << "  indexed 4-vertex/6-index background: " << (indexedQuad ? "PASS" : "FAIL") << '\n'
           << "  finite deterministic daylight colors: " << (finite && deterministic && daylight ? "PASS" : "FAIL") << '\n'
           << "  world horizon and below-horizon continuation: " << (worldHorizon ? "PASS" : "FAIL") << '\n'
           << "  translation invariant / pitch responsive / aspect aware: "
           << (translation && rotation && aspect ? "PASS" : "FAIL") << '\n'
           << "  nine camera presets cover all background rays: "
           << (presets ? "PASS" : "FAIL") << '\n'
           << (valid ? "Sky-background validation passed.\n" : "Sky-background validation failed.\n");
    return valid;
}
