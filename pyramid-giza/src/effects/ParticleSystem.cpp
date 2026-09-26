#include "effects/ParticleSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <stdexcept>

#include <glad/glad.h>
#include <glm/gtc/constants.hpp>

namespace
{
std::uint32_t hashValue(std::uint32_t value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    return value ^ (value >> 16u);
}

float unitRandom(std::uint32_t eventId, std::uint32_t particleIndex,
                 std::uint32_t channel)
{
    const std::uint32_t value = hashValue(
        eventId * 747796405u + particleIndex * 2891336453u + channel * 277803737u);
    return static_cast<float>(value & 0x00ffffffu) / 16777215.0f;
}

float signedRandom(std::uint32_t eventId, std::uint32_t particleIndex,
                   std::uint32_t channel)
{
    return unitRandom(eventId, particleIndex, channel) * 2.0f - 1.0f;
}

bool finiteParticle(const Particle& particle)
{
    const auto finite3 = [](const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z);
    };
    return finite3(particle.position) && finite3(particle.velocity) &&
           finite3(particle.color) && std::isfinite(particle.age) &&
           std::isfinite(particle.lifetime) && std::isfinite(particle.startSize) &&
           std::isfinite(particle.endSize) && std::isfinite(particle.startAlpha) &&
           std::isfinite(particle.endAlpha) && std::isfinite(particle.rotation) &&
           std::isfinite(particle.angularVelocity);
}
} // namespace

ParticleSystem::ParticleSystem(std::size_t capacity)
    : particles_(std::max<std::size_t>(1, capacity))
{
    gpuInstances_.reserve(particles_.size());
}

ParticleSystem::~ParticleSystem()
{
    if (instanceVbo_ != 0)
        glDeleteBuffers(1, &instanceVbo_);
}

void ParticleSystem::initializeGpu()
{
    MeshData quad("ParticleBillboardQuad");
    quad.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
    quad.indices = {0, 1, 2, 0, 2, 3};
    quad_.upload(quad);
    shader_ = std::make_unique<Shader>("shaders/particle.vert", "shaders/particle.frag");

    TextureData texture;
    texture.width = textureSize;
    texture.height = textureSize;
    texture.pixels.resize(static_cast<std::size_t>(textureSize * textureSize * 3));
    for (int y = 0; y < textureSize; ++y)
        for (int x = 0; x < textureSize; ++x)
        {
            const glm::vec2 uv{(x + 0.5f) / textureSize * 2.0f - 1.0f,
                               (y + 0.5f) / textureSize * 2.0f - 1.0f};
            const float edge = std::clamp(1.0f - glm::length(uv), 0.0f, 1.0f);
            const float alpha = edge * edge * (3.0f - 2.0f * edge);
            const unsigned char value = static_cast<unsigned char>(alpha * 255.0f + 0.5f);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * textureSize + x) * 3u;
            texture.pixels[offset] = value;
            texture.pixels[offset + 1] = value;
            texture.pixels[offset + 2] = value;
        }
    softTexture_.upload(texture);

    glGenBuffers(1, &instanceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(particles_.size() * sizeof(GpuInstance)),
                 nullptr, GL_DYNAMIC_DRAW);
    gpuInitialized_ = true;
}

void ParticleSystem::clear()
{
    for (Particle& particle : particles_)
        particle.alive = false;
    nextSlot_ = 0;
    emittedThisFrame_ = 0;
    peakActiveCount_ = 0;
}

std::size_t ParticleSystem::findReusableSlot()
{
    for (std::size_t offset = 0; offset < particles_.size(); ++offset)
    {
        const std::size_t index = (nextSlot_ + offset) % particles_.size();
        if (!particles_[index].alive)
        {
            nextSlot_ = (index + 1) % particles_.size();
            return index;
        }
    }
    return particles_.size();
}

std::size_t ParticleSystem::emitBurst(const ParticleEmission& emission,
                                      std::size_t count, std::uint32_t eventId)
{
    std::size_t emitted = 0;
    for (std::size_t particleIndex = 0; particleIndex < count; ++particleIndex)
    {
        const std::size_t slot = findReusableSlot();
        if (slot == particles_.size())
        {
            rejectedTotal_ += count - particleIndex;
            break;
        }
        Particle& particle = particles_[slot];
        const std::uint32_t index = static_cast<std::uint32_t>(particleIndex);
        particle.position = emission.origin + emission.positionSpread * glm::vec3{
            signedRandom(eventId, index, 0), unitRandom(eventId, index, 1),
            signedRandom(eventId, index, 2)};
        particle.velocity = emission.baseVelocity + emission.velocitySpread * glm::vec3{
            signedRandom(eventId, index, 3), signedRandom(eventId, index, 4),
            signedRandom(eventId, index, 5)};
        particle.color = emission.color *
            (0.90f + 0.10f * unitRandom(eventId, index, 6));
        particle.age = 0.0f;
        particle.lifetime = glm::mix(emission.lifetimeMin, emission.lifetimeMax,
                                     unitRandom(eventId, index, 7));
        particle.startSize = glm::mix(emission.startSizeMin, emission.startSizeMax,
                                      unitRandom(eventId, index, 8));
        particle.endSize = particle.startSize * emission.endSizeMultiplier;
        particle.startAlpha = emission.startAlpha;
        particle.endAlpha = 0.0f;
        particle.rotation = unitRandom(eventId, index, 9) * glm::two_pi<float>();
        particle.angularVelocity = signedRandom(eventId, index, 10) * 0.8f;
        particle.drag = emission.drag;
        particle.gravity = emission.gravity;
        particle.kind = emission.kind;
        particle.alive = true;
        ++emitted;
    }
    emittedThisFrame_ += emitted;
    return emitted;
}

void ParticleSystem::update(float deltaTime)
{
    const auto start = std::chrono::steady_clock::now();
    emittedThisFrame_ = 0;
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;
    const float dt = std::min(deltaTime, 0.1f);
    for (Particle& particle : particles_)
    {
        if (!particle.alive) continue;
        particle.age += dt;
        if (particle.age >= particle.lifetime)
        {
            particle.age = particle.lifetime;
            particle.alive = false;
            continue;
        }
        if (particle.drag > 1.0e-5f)
        {
            const float decay = std::exp(-particle.drag * dt);
            const glm::vec2 horizontal{particle.velocity.x, particle.velocity.z};
            const glm::vec2 displacement = horizontal * ((1.0f - decay) / particle.drag);
            particle.position.x += displacement.x;
            particle.position.z += displacement.y;
            particle.velocity.x *= decay;
            particle.velocity.z *= decay;
        }
        else
        {
            particle.position.x += particle.velocity.x * dt;
            particle.position.z += particle.velocity.z * dt;
        }
        particle.position.y += particle.velocity.y * dt +
                               0.5f * particle.gravity * dt * dt;
        particle.velocity.y += particle.gravity * dt;
        particle.rotation += particle.angularVelocity * dt;
    }
    lastUpdateMilliseconds_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

std::size_t ParticleSystem::activeCount() const
{
    return static_cast<std::size_t>(std::count_if(
        particles_.begin(), particles_.end(),
        [](const Particle& particle) { return particle.alive; }));
}

void ParticleSystem::configureInstanceAttributes() const
{
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    const GLsizei stride = static_cast<GLsizei>(sizeof(GpuInstance));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(GpuInstance, centerSize)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(GpuInstance, colorAlpha)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(GpuInstance, rotation)));
    glVertexAttribDivisor(5, 1);
}

ParticleRenderResult ParticleSystem::render(const glm::mat4& view,
                                             const glm::mat4& projection,
                                             const glm::vec3& cameraPosition,
                                             const glm::vec3& lightingTint)
{
    if (!gpuInitialized_)
        return {};
    const std::size_t currentActiveCount = activeCount();
    peakActiveCount_ = std::max(peakActiveCount_, currentActiveCount);
    if (currentActiveCount == 0)
        return {};
    std::vector<const Particle*> sorted;
    sorted.reserve(particles_.size());
    for (const Particle& particle : particles_)
        if (particle.alive) sorted.push_back(&particle);
    std::sort(sorted.begin(), sorted.end(), [&](const Particle* left, const Particle* right)
    {
        const glm::vec3 leftOffset = left->position - cameraPosition;
        const glm::vec3 rightOffset = right->position - cameraPosition;
        return glm::dot(leftOffset, leftOffset) >
               glm::dot(rightOffset, rightOffset);
    });

    gpuInstances_.clear();
    for (const Particle* particle : sorted)
    {
        const float normalizedAge = particle->age / particle->lifetime;
        const float size = glm::mix(particle->startSize, particle->endSize, normalizedAge);
        const float alpha = glm::mix(particle->startAlpha, particle->endAlpha,
                                     normalizedAge);
        gpuInstances_.push_back({{particle->position, size},
                                 {particle->color, alpha}, particle->rotation});
    }
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(gpuInstances_.size() * sizeof(GpuInstance)),
                    gpuInstances_.data());

    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    GLint previousPolygonMode[2] = {GL_FILL, GL_FILL};
    glGetIntegerv(GL_POLYGON_MODE, previousPolygonMode);
    GLint previousBlendSource = GL_ONE;
    GLint previousBlendDestination = GL_ZERO;
    GLint previousBlendSourceAlpha = GL_ONE;
    GLint previousBlendDestinationAlpha = GL_ZERO;
    GLint previousBlendEquationRgb = GL_FUNC_ADD;
    GLint previousBlendEquationAlpha = GL_FUNC_ADD;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSource);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDestination);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDestinationAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &previousBlendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &previousBlendEquationAlpha);
    GLint previousProgram = 0;
    GLint previousVertexArray = 0;
    GLint previousArrayBuffer = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    GLint previousActiveTexture = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE2);
    GLint previousParticleTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousParticleTexture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader_->use();
    shader_->setMat4("view", view);
    shader_->setMat4("projection", projection);
    shader_->setVec3("cameraRight", {view[0][0], view[1][0], view[2][0]});
    shader_->setVec3("cameraUp", {view[0][1], view[1][1], view[2][1]});
    shader_->setVec3("lightingTint", lightingTint);
    shader_->setInt("particleTexture", 2);
    softTexture_.bind(2);
    quad_.bind();
    configureInstanceAttributes();
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(quad_.indexCount()),
                            GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(gpuInstances_.size()));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousParticleTexture));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSource),
                        static_cast<GLenum>(previousBlendDestination),
                        static_cast<GLenum>(previousBlendSourceAlpha),
                        static_cast<GLenum>(previousBlendDestinationAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(previousBlendEquationRgb),
                            static_cast<GLenum>(previousBlendEquationAlpha));
    if (!blendWasEnabled) glDisable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glDepthMask(previousDepthMask);
    // Core OpenGL accepts GL_FRONT_AND_BACK for polygon mode; the project
    // always configures both faces together.
    glPolygonMode(GL_FRONT_AND_BACK, previousPolygonMode[0]);
    glUseProgram(static_cast<GLuint>(previousProgram));
    glBindVertexArray(static_cast<GLuint>(previousVertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    return {1, gpuInstances_.size(), gpuInstances_.size() * 2u};
}

std::size_t ParticleSystem::instanceBufferBytes() const
{
    return particles_.size() * sizeof(GpuInstance);
}

std::size_t ParticleSystem::textureBytes() const
{
    return static_cast<std::size_t>(textureSize * textureSize * 3 * 4 / 3);
}

int malletImpactEvents(float previousElapsed, float currentElapsed)
{
    if (!std::isfinite(previousElapsed) || !std::isfinite(currentElapsed) ||
        currentElapsed <= previousElapsed)
        return 0;
    constexpr float cyclesPerSecond = 0.72f;
    constexpr float impactPhase = 0.62f;
    const int previousIndex = static_cast<int>(
        std::floor(previousElapsed * cyclesPerSecond - impactPhase));
    const int currentIndex = static_cast<int>(
        std::floor(currentElapsed * cyclesPerSecond - impactPhase));
    return std::max(0, currentIndex - previousIndex);
}

bool placementSettlementCrossed(float previousProgress, float currentProgress,
                                float stableThreshold)
{
    return std::isfinite(previousProgress) && std::isfinite(currentProgress) &&
           std::isfinite(stableThreshold) && currentProgress > previousProgress &&
           stableThreshold > previousProgress + 1.0e-6f &&
           stableThreshold <= currentProgress + 1.0e-6f;
}

std::size_t sledgeDustEmissionCount(float speed, float deltaTime, float& accumulator)
{
    if (!std::isfinite(speed) || !std::isfinite(deltaTime) || speed < 0.05f ||
        deltaTime <= 0.0f)
        return 0;
    const float rate = std::clamp(speed * 3.0f, 0.0f, 22.0f);
    accumulator += std::min(rate * deltaTime, 6.0f);
    const std::size_t count = static_cast<std::size_t>(std::floor(accumulator));
    accumulator -= static_cast<float>(count);
    return count;
}

glm::vec2 waterUvOffset(float environmentTime)
{
    const float wrapped = std::fmod(std::max(0.0f, environmentTime), 400.0f);
    return {wrapped * 0.0018f, wrapped * 0.00065f};
}

float treeSwayDegrees(float environmentTime, std::size_t treeIndex)
{
    const float phase = static_cast<float>((treeIndex * 37u) % 101u) * 0.071f;
    return 1.35f * std::sin(environmentTime * 0.72f + phase);
}

bool validatePhase11Particles(std::ostream& output)
{
    ParticleEmission emission;
    emission.origin = {1.0f, 2.0f, 3.0f};
    ParticleSystem first(16);
    ParticleSystem second(16);
    const bool emitted = first.emitBurst(emission, 12, 99u) == 12 &&
                         second.emitBurst(emission, 12, 99u) == 12;
    bool deterministic = emitted;
    for (std::size_t index = 0; index < first.particles().size(); ++index)
    {
        const Particle& a = first.particles()[index];
        const Particle& b = second.particles()[index];
        deterministic = deterministic && a.alive == b.alive &&
                        a.position == b.position && a.velocity == b.velocity &&
                        a.lifetime == b.lifetime && a.startSize == b.startSize;
    }
    for (int step = 0; step < 60; ++step) first.update(1.0f / 60.0f);
    for (int step = 0; step < 30; ++step) second.update(1.0f / 30.0f);
    bool finiteAndEquivalent = first.activeCount() <= first.capacity();
    for (std::size_t index = 0; index < first.particles().size(); ++index)
    {
        const Particle& a = first.particles()[index];
        const Particle& b = second.particles()[index];
        if (!a.alive && !b.alive) continue;
        finiteAndEquivalent = finiteAndEquivalent && finiteParticle(a) &&
                              a.age >= 0.0f && a.age <= a.lifetime &&
                              glm::distance(a.position, b.position) < 1.0e-3f;
    }
    ParticleSystem pool(4);
    const std::size_t capped = pool.emitBurst(emission, 9, 7u);
    const bool poolSafe = capped == 4 && pool.activeCount() == 4 &&
                          pool.rejectedTotal() == 5;
    ParticleSystem resetPool(4);
    resetPool.emitBurst(emission, 4, 17u);
    const Particle beforeReset = resetPool.particles()[0];
    resetPool.clear();
    const bool cleared = resetPool.activeCount() == 0;
    resetPool.emitBurst(emission, 1, 17u);
    const Particle& afterReset = resetPool.particles()[0];
    const bool resetPredictable = cleared && afterReset.alive &&
                                  beforeReset.position == afterReset.position &&
                                  beforeReset.velocity == afterReset.velocity;
    const bool valid = deterministic && finiteAndEquivalent && poolSafe &&
                       resetPredictable;
    output << "Phase 11 particle validation\n"
           << "  deterministic fixed-capacity emission: "
           << (deterministic ? "PASS" : "FAIL") << '\n'
           << "  finite lifetime and frame-independent integration: "
           << (finiteAndEquivalent ? "PASS" : "FAIL") << '\n'
           << "  capacity overflow safely capped: "
           << (poolSafe ? "PASS" : "FAIL") << '\n'
           << "  clear/reset removes stale particles and reuses pool: "
           << (resetPredictable ? "PASS" : "FAIL") << '\n'
           << (valid ? "Particle checks passed.\n" : "Particle checks failed.\n");
    return valid;
}

bool validatePhase11EffectEvents(std::ostream& output)
{
    const bool mallet = malletImpactEvents(0.80f, 0.90f) == 1 &&
                        malletImpactEvents(0.90f, 0.95f) == 0;
    const bool placement = placementSettlementCrossed(0.40f, 0.43f, 0.42f) &&
                           !placementSettlementCrossed(0.43f, 0.45f, 0.42f) &&
                           !placementSettlementCrossed(0.90f, 0.20f, 0.42f);
    float accumulator = 0.0f;
    const std::size_t stationary = sledgeDustEmissionCount(0.0f, 1.0f, accumulator);
    std::size_t moving = 0;
    for (int step = 0; step < 60; ++step)
        moving += sledgeDustEmissionCount(2.0f, 1.0f / 60.0f, accumulator);
    const bool sledge = stationary == 0 && moving > 0 && moving <= 22;
    const bool valid = mallet && placement && sledge;
    output << "Phase 11 effect-event validation\n"
           << "  mallet crossing emits exactly one burst: "
           << (mallet ? "PASS" : "FAIL") << '\n'
           << "  settlement crossing is edge-triggered: "
           << (placement ? "PASS" : "FAIL") << '\n'
           << "  stationary/moving sledge emission: "
           << (sledge ? "PASS" : "FAIL") << '\n'
           << (valid ? "Effect-event checks passed.\n"
                     : "Effect-event checks failed.\n");
    return valid;
}

bool validatePhase11EnvironmentMotion(std::ostream& output)
{
    bool water = true;
    bool trees = true;
    for (float time : {0.0f, 1.0f, 1000.0f, 100000.0f})
    {
        const glm::vec2 first = waterUvOffset(time);
        const glm::vec2 second = waterUvOffset(time);
        water = water && first == second && std::isfinite(first.x) &&
                std::isfinite(first.y) && first.x >= 0.0f && first.x < 0.73f;
        for (std::size_t tree = 0; tree < 18; ++tree)
        {
            const float angle = treeSwayDegrees(time, tree);
            trees = trees && std::isfinite(angle) && std::abs(angle) <= 1.3501f &&
                    angle == treeSwayDegrees(time, tree);
        }
    }
    const bool valid = water && trees;
    output << "Phase 11 environment-motion validation\n"
           << "  wrapped deterministic Nile UV motion: "
           << (water ? "PASS" : "FAIL") << '\n'
           << "  bounded deterministic tree sway: "
           << (trees ? "PASS" : "FAIL") << '\n'
           << (valid ? "Environment-motion checks passed.\n"
                     : "Environment-motion checks failed.\n");
    return valid;
}
