#pragma once

#include <array>
#include <iosfwd>

#include <glm/glm.hpp>

struct BoundingSphere
{
    glm::vec3 center{0.0f};
    float radius = 0.0f;
};

class Frustum
{
public:
    static Frustum fromMatrix(const glm::mat4& clipMatrix);

    bool intersects(const BoundingSphere& sphere, float margin = 0.0f) const;
    bool containsPoint(const glm::vec3& point) const;
    const std::array<glm::vec4, 6>& planes() const { return planes_; }

private:
    std::array<glm::vec4, 6> planes_{};
};

BoundingSphere transformedPrimitiveBounds(const glm::mat4& model, float localRadius);
bool validatePhase10Frustum(std::ostream& output);
