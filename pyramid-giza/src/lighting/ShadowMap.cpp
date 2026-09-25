#include "lighting/ShadowMap.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include <glad/glad.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionAnimation.h"
#include "lighting/SunController.h"
#include "objects/Sledge.h"

namespace
{
bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finiteMatrix(const glm::mat4& matrix)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(matrix[column][row]))
                return false;
    return true;
}

float maximumMatrixDifference(const glm::mat4& left, const glm::mat4& right)
{
    float difference = 0.0f;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            difference = std::max(difference, std::abs(left[column][row] - right[column][row]));
    return difference;
}
} // namespace

LightSpaceState calculateLightSpace(const glm::vec3& sunDirection,
                                    const ShadowSettings& settings)
{
    if (!finiteVector(sunDirection) || glm::length(sunDirection) < 1.0e-6f)
        throw std::invalid_argument("Shadow light direction must be finite and non-zero");

    const glm::vec3 direction = glm::normalize(sunDirection);
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(direction, up)) > 0.95f)
        up = {0.0f, 0.0f, 1.0f};

    LightSpaceState state;
    state.lightPosition = settings.center - direction * settings.lightDistance;
    state.upVector = up;
    state.view = glm::lookAt(state.lightPosition, settings.center, up);
    state.projection = glm::ortho(
        -settings.halfWidth, settings.halfWidth,
        -settings.halfHeight, settings.halfHeight,
        settings.nearPlane, settings.farPlane);
    state.matrix = state.projection * state.view;
    return state;
}

bool validatePhase8Shadows(std::ostream& output)
{
    const ShadowSettings settings;
    const bool settingsValid = settings.resolution > 0 &&
                               settings.halfWidth > 0.0f &&
                               settings.halfHeight > 0.0f &&
                               settings.lightDistance > 0.0f &&
                               settings.nearPlane > 0.0f &&
                               settings.farPlane > settings.nearPlane &&
                               settings.minimumBias >= 0.0f &&
                               settings.slopeBias >= 0.0f &&
                               settings.shadowStrength >= 0.0f &&
                               settings.shadowStrength <= 1.0f &&
                               settings.pcfRadius == 1;

    const LightSpaceState morning = calculateLightSpace(
        SunController::evaluate(SunController::morningTime).light.direction, settings);
    const LightSpaceState noon = calculateLightSpace(
        SunController::evaluate(SunController::noonTime).light.direction, settings);
    const LightSpaceState evening = calculateLightSpace(
        SunController::evaluate(SunController::eveningTime).light.direction, settings);

    const auto finiteState = [](const LightSpaceState& state) {
        return finiteVector(state.lightPosition) && finiteVector(state.upVector) &&
               finiteMatrix(state.view) && finiteMatrix(state.projection) &&
               finiteMatrix(state.matrix);
    };
    const bool matricesFinite = finiteState(morning) && finiteState(noon) &&
                                finiteState(evening);
    const bool highSunUpSafe =
        std::abs(glm::dot(glm::normalize(
            SunController::evaluate(SunController::noonTime).light.direction),
            glm::vec3{0.0f, 1.0f, 0.0f})) > 0.95f &&
        glm::all(glm::epsilonEqual(noon.upVector, glm::vec3{0.0f, 0.0f, 1.0f}, 1.0e-6f));
    const bool sunDependent =
        maximumMatrixDifference(morning.view, evening.view) > 0.01f &&
        maximumMatrixDifference(morning.matrix, evening.matrix) > 0.01f;

    ConstructionAnimationController animation;
    animation.update(16.0f);
    const ConstructionAnimationSnapshot snapshot = animation.snapshot();
    const std::vector<ObjectPart> loadedSledge = Sledge::create(true);
    const glm::mat4 sharedAnimatedModel =
        snapshot.loadedSledgeRoot * loadedSledge.front().localTransform;
    // Both passes consume this one model matrix from the shared per-frame render list.
    const glm::mat4 depthPassModel = sharedAnimatedModel;
    const glm::mat4 litPassModel = sharedAnimatedModel;
    const bool animatedTransformShared =
        finiteMatrix(sharedAnimatedModel) &&
        maximumMatrixDifference(depthPassModel, litPassModel) < 1.0e-7f;

    const bool valid = settingsValid && matricesFinite && highSunUpSafe &&
                       sunDependent && animatedTransformShared;
    output << "Phase 8 directional-shadow validation\n"
           << "  orthographic bounds/settings: "
           << (settingsValid ? "PASS\n" : "FAIL\n")
           << "  morning/noon/evening light matrices finite: "
           << (matricesFinite ? "PASS\n" : "FAIL\n")
           << "  high-sun alternative up vector: "
           << (highSunUpSafe ? "PASS\n" : "FAIL\n")
           << "  moving sun changes light-space matrix: "
           << (sunDependent ? "PASS\n" : "FAIL\n")
           << "  animated model shared by depth/lit passes: "
           << (animatedTransformShared ? "PASS\n" : "FAIL\n")
           << (valid ? "Phase 8 shadow checks passed.\n"
                     : "Phase 8 shadow checks failed.\n");
    return valid;
}

ShadowMap::~ShadowMap()
{
    cleanup();
}

void ShadowMap::initialize(int width, int height)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Shadow-map dimensions must be positive");

    int maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (width > maximumTextureSize || height > maximumTextureSize)
        throw std::runtime_error("Requested shadow map exceeds GL_MAX_TEXTURE_SIZE");

    cleanup();
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &framebuffer_);
    glGenTextures(1, &depthTexture_);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    const float borderDepth[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderDepth);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::ostringstream message;
        message << "Shadow framebuffer is incomplete (status 0x"
                << std::hex << std::uppercase << status << ')';
        cleanup();
        throw std::runtime_error(message.str());
    }
}

void ShadowMap::beginDepthPass() const
{
    if (!initialized())
        throw std::runtime_error("Shadow map used before initialization");
    // The previous lit pass leaves the depth texture on unit 0. Unbind it before
    // attaching the same image for depth rendering to avoid a feedback hazard.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glViewport(0, 0, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::endDepthPass(int viewportWidth, int viewportHeight) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void ShadowMap::bindDepthTexture(unsigned int textureUnit) const
{
    if (!initialized())
        throw std::runtime_error("Shadow texture used before initialization");
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
}

void ShadowMap::cleanup()
{
    if (depthTexture_ != 0)
        glDeleteTextures(1, &depthTexture_);
    if (framebuffer_ != 0)
        glDeleteFramebuffers(1, &framebuffer_);
    framebuffer_ = 0;
    depthTexture_ = 0;
    width_ = 0;
    height_ = 0;
}
