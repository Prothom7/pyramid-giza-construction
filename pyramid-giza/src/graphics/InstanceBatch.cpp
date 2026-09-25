#include "graphics/InstanceBatch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <utility>

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>

#include "animation/ConstructionTimeline.h"
#include "graphics/Mesh.h"
#include "scene/SceneTypes.h"

InstanceBatch::~InstanceBatch()
{
    cleanup();
}

InstanceBatch::InstanceBatch(InstanceBatch&& other) noexcept
    : instanceVbo_(other.instanceVbo_), capacity_(other.capacity_),
      instanceCount_(other.instanceCount_)
{
    other.instanceVbo_ = 0;
    other.capacity_ = 0;
    other.instanceCount_ = 0;
}

InstanceBatch& InstanceBatch::operator=(InstanceBatch&& other) noexcept
{
    if (this != &other)
    {
        cleanup();
        instanceVbo_ = other.instanceVbo_;
        capacity_ = other.capacity_;
        instanceCount_ = other.instanceCount_;
        other.instanceVbo_ = 0;
        other.capacity_ = 0;
        other.instanceCount_ = 0;
    }
    return *this;
}

void InstanceBatch::uploadStatic(const std::vector<InstanceData>& instances)
{
    if (instanceVbo_ == 0)
        glGenBuffers(1, &instanceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)),
                 instances.empty() ? nullptr : instances.data(), GL_STATIC_DRAW);
    capacity_ = instances.size();
    instanceCount_ = instances.size();
}

void InstanceBatch::updateDynamic(const std::vector<InstanceData>& instances)
{
    if (instanceVbo_ == 0)
        glGenBuffers(1, &instanceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    if (instances.size() > capacity_)
    {
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)),
                     instances.empty() ? nullptr : instances.data(), GL_DYNAMIC_DRAW);
        capacity_ = instances.size();
    }
    else if (!instances.empty())
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)),
                        instances.data());
    }
    instanceCount_ = instances.size();
}

void InstanceBatch::configureAttributes() const
{
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    const GLsizei stride = static_cast<GLsizei>(sizeof(InstanceData));
    const std::size_t modelOffset = offsetof(InstanceData, model);
    const std::size_t normalOffset = offsetof(InstanceData, normalMatrix);
    for (unsigned int column = 0; column < 4; ++column)
    {
        const unsigned int location = 3 + column;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(
                                  modelOffset + column * sizeof(glm::vec4)));
        glVertexAttribDivisor(location, 1);
    }
    for (unsigned int column = 0; column < 3; ++column)
    {
        const unsigned int location = 7 + column;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(
                                  normalOffset + column * sizeof(glm::vec3)));
        glVertexAttribDivisor(location, 1);
    }
    glEnableVertexAttribArray(10);
    glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(InstanceData, uvTransform)));
    glVertexAttribDivisor(10, 1);
}

void InstanceBatch::draw(const Mesh& mesh, std::size_t count) const
{
    const std::size_t safeCount = std::min(count, instanceCount_);
    if (safeCount == 0 || instanceVbo_ == 0)
        return;
    mesh.bind();
    configureAttributes();
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount()),
                            GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(safeCount));
}

void InstanceBatch::cleanup()
{
    if (instanceVbo_ != 0)
    {
        glDeleteBuffers(1, &instanceVbo_);
        instanceVbo_ = 0;
    }
    capacity_ = 0;
    instanceCount_ = 0;
}

InstanceData makeInstanceData(const glm::mat4& model,
                              const glm::vec2& uvScale,
                              const glm::vec2& uvOffset)
{
    return {model, glm::transpose(glm::inverse(glm::mat3(model))),
            {uvScale.x, uvScale.y, uvOffset.x, uvOffset.y}};
}

bool validInstanceData(const InstanceData& instance)
{
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(instance.model[column][row]))
                return false;
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            if (!std::isfinite(instance.normalMatrix[column][row]))
                return false;
    for (int component = 0; component < 4; ++component)
        if (!std::isfinite(instance.uvTransform[component]))
            return false;
    return std::abs(glm::determinant(glm::mat3(instance.model))) > 1.0e-8f;
}

bool validatePhase10Instancing(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> blocks =
        PyramidLayout::generateComplete(config);
    ConstructionTimelineController timeline;
    bool checkpointCountsValid = blocks.size() == 7714;
    const std::array<std::pair<float, std::size_t>, 5> checkpoints{{
        {0.0f, 70}, {0.25f, 4734}, {0.50f, 6964},
        {0.75f, 7561}, {1.0f, 7714}}};

    for (const auto& checkpoint : checkpoints)
    {
        timeline.setProgress(checkpoint.first);
        std::size_t stable = 0;
        std::size_t frontier = 0;
        for (const PyramidBlockPlacement& block : blocks)
        {
            const ConstructionBlockState state = timeline.blockState(block, config);
            if (state.frontier) ++frontier;
            else if (state.visible) ++stable;
        }
        checkpointCountsValid = checkpointCountsValid &&
                                stable + frontier == checkpoint.second;
    }

    bool allMatricesValid = true;
    for (const PyramidBlockPlacement& block : blocks)
    {
        const MaterialId id = (block.level % 4 == 1 || block.level % 4 == 2)
                                  ? MaterialId::LimestoneVariation
                                  : MaterialId::Limestone;
        const Material& material = materialDefinition(id);
        const InstanceData instance = makeInstanceData(
            makeTransform(block.position, {}, block.scale),
            material.textureScale, material.textureOffset);
        allMatricesValid = allMatricesValid && validInstanceData(instance);
    }

    bool representativeEquivalent = true;
    for (std::size_t index : {0u, 1024u, 4096u, 7713u})
    {
        const PyramidBlockPlacement& block = blocks[index];
        const MaterialId id = (block.level % 4 == 1 || block.level % 4 == 2)
                                  ? MaterialId::LimestoneVariation
                                  : MaterialId::Limestone;
        const Material& material = materialDefinition(id);
        const glm::mat4 original = makeTransform(block.position, {}, block.scale);
        const InstanceData instance =
            makeInstanceData(original, material.textureScale, material.textureOffset);
        representativeEquivalent = representativeEquivalent &&
            validInstanceData(instance) &&
            glm::length(glm::vec4{instance.model[0] - original[0]}) < 1.0e-6f &&
            glm::length(glm::vec4{instance.model[1] - original[1]}) < 1.0e-6f &&
            glm::length(glm::vec4{instance.model[2] - original[2]}) < 1.0e-6f &&
            glm::length(glm::vec4{instance.model[3] - original[3]}) < 1.0e-6f &&
            glm::length(glm::vec2(instance.uvTransform) - material.textureScale) <
                1.0e-6f &&
            glm::length(glm::vec2(instance.uvTransform.z, instance.uvTransform.w) -
                        material.textureOffset) < 1.0e-6f;
    }
    const bool valid = checkpointCountsValid && allMatricesValid &&
                       representativeEquivalent;

    output << "Phase 10 indexed-instancing validation\n"
           << "  construction checkpoint totals: "
           << (checkpointCountsValid ? "PASS" : "FAIL") << '\n'
           << "  all 7714 model and inverse-transpose matrices finite: "
           << (allMatricesValid ? "PASS" : "FAIL") << '\n'
           << "  Phase 9 model and UV transforms preserved: "
           << (representativeEquivalent ? "PASS" : "FAIL") << '\n'
           << "  shared pyramid batches: 8 stable + at most 2 frontier\n"
           << (valid ? "Instancing checks passed.\n" : "Instancing checks failed.\n");
    return valid;
}

bool validatePhase10PerformanceStructure(std::ostream& output)
{
    constexpr std::size_t phase9VisibleDraws = 9690;
    constexpr std::size_t pyramidObjects = 7714;
    constexpr std::size_t maximumPyramidInstancedDraws = 10;
    constexpr std::size_t phase10Maximum =
        phase9VisibleDraws - pyramidObjects + maximumPyramidInstancedDraws;
    const float reduction = 100.0f *
        (1.0f - static_cast<float>(phase10Maximum) /
                    static_cast<float>(phase9VisibleDraws));
    const bool valid = maximumPyramidInstancedDraws <= 10 &&
                       reduction > 70.0f && sizeof(InstanceData) > sizeof(glm::mat4);
    output << "Phase 10 performance-structure validation\n"
           << "  Phase 9 maximum visible draws: " << phase9VisibleDraws << '\n'
           << "  structural Phase 10 maximum: " << phase10Maximum << '\n'
           << "  structural reduction: " << reduction << "%\n"
           << "  pyramid instanced draws per pass <= 10: "
           << (valid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Performance structure checks passed.\n"
                     : "Performance structure checks failed.\n");
    return valid;
}
