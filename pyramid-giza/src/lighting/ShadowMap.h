#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

struct ShadowSettings
{
    static constexpr int defaultResolution = 4096;

    int resolution = defaultResolution;
    glm::vec3 center{-20.0f, 18.0f, -8.0f};
    float halfWidth = 235.0f;
    float halfHeight = 235.0f;
    float lightDistance = 320.0f;
    float nearPlane = 1.0f;
    float farPlane = 700.0f;
    float minimumBias = 0.00035f;
    float slopeBias = 0.0025f;
    float shadowStrength = 0.90f;
    int pcfRadius = 1;
};

struct LightSpaceState
{
    glm::vec3 lightPosition{0.0f};
    glm::vec3 upVector{0.0f, 1.0f, 0.0f};
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 matrix{1.0f};
};

enum class ShadowDebugMode
{
    Normal = 0,
    Factor,
    Count
};

LightSpaceState calculateLightSpace(const glm::vec3& sunDirection,
                                    const ShadowSettings& settings);
bool validatePhase8Shadows(std::ostream& output);

class ShadowMap
{
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    void initialize(int width, int height);
    void beginDepthPass() const;
    void endDepthPass(int viewportWidth, int viewportHeight) const;
    void bindDepthTexture(unsigned int textureUnit) const;
    void cleanup();

    unsigned int depthTexture() const { return depthTexture_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool initialized() const { return framebuffer_ != 0 && depthTexture_ != 0; }

private:
    unsigned int framebuffer_ = 0;
    unsigned int depthTexture_ = 0;
    int width_ = 0;
    int height_ = 0;
};
