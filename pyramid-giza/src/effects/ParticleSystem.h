#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"
#include "graphics/Texture.h"

enum class ParticleKind { SandDust, StoneDust, PlacementDust, AmbientDust };

struct Particle
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 color{1.0f};
    float age = 0.0f;
    float lifetime = 1.0f;
    float startSize = 0.1f;
    float endSize = 0.4f;
    float startAlpha = 0.2f;
    float endAlpha = 0.0f;
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float drag = 0.7f;
    float gravity = -0.35f;
    ParticleKind kind = ParticleKind::SandDust;
    bool alive = false;
};

struct ParticleEmission
{
    glm::vec3 origin{0.0f};
    glm::vec3 positionSpread{0.1f};
    glm::vec3 baseVelocity{0.0f, 0.4f, 0.0f};
    glm::vec3 velocitySpread{0.1f};
    glm::vec3 color{0.78f, 0.62f, 0.38f};
    float lifetimeMin = 0.7f;
    float lifetimeMax = 1.4f;
    float startSizeMin = 0.10f;
    float startSizeMax = 0.24f;
    float endSizeMultiplier = 2.4f;
    float startAlpha = 0.24f;
    float drag = 0.8f;
    float gravity = -0.32f;
    ParticleKind kind = ParticleKind::SandDust;
};

struct ParticleRenderResult
{
    std::size_t drawCalls = 0;
    std::size_t instances = 0;
    std::size_t triangles = 0;
};

class ParticleSystem
{
public:
    static constexpr std::size_t defaultCapacity = 512;
    static constexpr int textureSize = 64;

    explicit ParticleSystem(std::size_t capacity = defaultCapacity);
    ~ParticleSystem();
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    void initializeGpu();
    void clear();
    void update(float deltaTime);
    std::size_t emitBurst(const ParticleEmission& emission, std::size_t count,
                          std::uint32_t eventId);
    ParticleRenderResult render(const glm::mat4& view, const glm::mat4& projection,
                                const glm::vec3& cameraPosition,
                                const glm::vec3& lightingTint);

    std::size_t capacity() const { return particles_.size(); }
    std::size_t activeCount() const;
    std::size_t peakActiveCount() const { return peakActiveCount_; }
    std::size_t emittedThisFrame() const { return emittedThisFrame_; }
    std::size_t rejectedTotal() const { return rejectedTotal_; }
    std::size_t cpuPoolBytes() const { return particles_.capacity() * sizeof(Particle); }
    std::size_t instanceBufferBytes() const;
    std::size_t textureBytes() const;
    double lastUpdateMilliseconds() const { return lastUpdateMilliseconds_; }
    const std::vector<Particle>& particles() const { return particles_; }

private:
    struct GpuInstance
    {
        glm::vec4 centerSize{0.0f};
        glm::vec4 colorAlpha{1.0f};
        float rotation = 0.0f;
    };
    std::size_t findReusableSlot();
    void configureInstanceAttributes() const;

    std::vector<Particle> particles_;
    std::vector<GpuInstance> gpuInstances_;
    std::size_t nextSlot_ = 0;
    std::size_t emittedThisFrame_ = 0;
    std::size_t rejectedTotal_ = 0;
    std::size_t peakActiveCount_ = 0;
    double lastUpdateMilliseconds_ = 0.0;
    Mesh quad_;
    Texture softTexture_;
    std::unique_ptr<Shader> shader_;
    unsigned int instanceVbo_ = 0;
    bool gpuInitialized_ = false;
};

int malletImpactEvents(float previousElapsed, float currentElapsed);
bool placementSettlementCrossed(float previousProgress, float currentProgress,
                                float stableThreshold);
std::size_t sledgeDustEmissionCount(float speed, float deltaTime, float& accumulator);
glm::vec2 waterUvOffset(float environmentTime);
float treeSwayDegrees(float environmentTime, std::size_t treeIndex);

bool validatePhase11Particles(std::ostream& output);
bool validatePhase11EffectEvents(std::ostream& output);
bool validatePhase11EnvironmentMotion(std::ostream& output);
