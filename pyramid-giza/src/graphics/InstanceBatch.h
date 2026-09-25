#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "graphics/Frustum.h"

class Mesh;

struct InstanceData
{
    glm::mat4 model{1.0f};
    glm::mat3 normalMatrix{1.0f};
    glm::vec4 uvTransform{1.0f, 1.0f, 0.0f, 0.0f};
};

class InstanceBatch
{
public:
    InstanceBatch() = default;
    ~InstanceBatch();
    InstanceBatch(const InstanceBatch&) = delete;
    InstanceBatch& operator=(const InstanceBatch&) = delete;
    InstanceBatch(InstanceBatch&& other) noexcept;
    InstanceBatch& operator=(InstanceBatch&& other) noexcept;

    void uploadStatic(const std::vector<InstanceData>& instances);
    void updateDynamic(const std::vector<InstanceData>& instances);
    void draw(const Mesh& mesh, std::size_t count) const;
    void cleanup();

    std::size_t capacity() const { return capacity_; }
    std::size_t instanceCount() const { return instanceCount_; }
    std::size_t byteCapacity() const { return capacity_ * sizeof(InstanceData); }

private:
    void configureAttributes() const;

    unsigned int instanceVbo_ = 0;
    std::size_t capacity_ = 0;
    std::size_t instanceCount_ = 0;
};

InstanceData makeInstanceData(const glm::mat4& model,
                              const glm::vec2& uvScale,
                              const glm::vec2& uvOffset);
bool validInstanceData(const InstanceData& instance);
bool validatePhase10Instancing(std::ostream& output);
bool validatePhase10PerformanceStructure(std::ostream& output);
