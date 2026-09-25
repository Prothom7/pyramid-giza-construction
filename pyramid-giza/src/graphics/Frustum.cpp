#include "graphics/Frustum.h"

#include <cmath>
#include <ostream>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
glm::vec4 normalizedPlane(const glm::vec4& plane)
{
    const float length = glm::length(glm::vec3{plane});
    return length > 1.0e-7f ? plane / length : glm::vec4{0.0f};
}

glm::vec4 row(const glm::mat4& matrix, int index)
{
    return {matrix[0][index], matrix[1][index],
            matrix[2][index], matrix[3][index]};
}
}

Frustum Frustum::fromMatrix(const glm::mat4& clipMatrix)
{
    Frustum frustum;
    const glm::vec4 r0 = row(clipMatrix, 0);
    const glm::vec4 r1 = row(clipMatrix, 1);
    const glm::vec4 r2 = row(clipMatrix, 2);
    const glm::vec4 r3 = row(clipMatrix, 3);
    frustum.planes_ = {
        normalizedPlane(r3 + r0), normalizedPlane(r3 - r0),
        normalizedPlane(r3 + r1), normalizedPlane(r3 - r1),
        normalizedPlane(r3 + r2), normalizedPlane(r3 - r2)};
    return frustum;
}

bool Frustum::intersects(const BoundingSphere& sphere, float margin) const
{
    const float conservativeRadius = std::max(0.0f, sphere.radius + margin);
    for (const glm::vec4& plane : planes_)
        if (glm::dot(glm::vec3{plane}, sphere.center) + plane.w <
            -conservativeRadius)
            return false;
    return true;
}

bool Frustum::containsPoint(const glm::vec3& point) const
{
    return intersects({point, 0.0f});
}

BoundingSphere transformedPrimitiveBounds(const glm::mat4& model, float localRadius)
{
    // The Frobenius norm is a conservative upper bound on the linear part's
    // spectral norm, including transforms that contain inherited shear.
    float squaredFrobeniusNorm = 0.0f;
    for (int column = 0; column < 3; ++column)
        squaredFrobeniusNorm += glm::dot(glm::vec3{model[column]},
                                        glm::vec3{model[column]});
    return {glm::vec3{model[3]},
            localRadius * std::sqrt(squaredFrobeniusNorm)};
}

bool validatePhase10Frustum(std::ostream& output)
{
    const glm::mat4 projection = glm::perspective(
        glm::radians(60.0f), 16.0f / 9.0f, 0.7f, 700.0f);
    const glm::mat4 view = glm::lookAt(glm::vec3{0.0f, 4.0f, 12.0f},
                                      glm::vec3{0.0f, 2.0f, 0.0f},
                                      glm::vec3{0.0f, 1.0f, 0.0f});
    const Frustum frustum = Frustum::fromMatrix(projection * view);
    bool planesValid = true;
    for (const glm::vec4& plane : frustum.planes())
    {
        planesValid = planesValid && std::isfinite(plane.x) &&
                      std::isfinite(plane.y) && std::isfinite(plane.z) &&
                      std::isfinite(plane.w);
        planesValid = planesValid &&
                      std::abs(glm::length(glm::vec3{plane}) - 1.0f) < 1.0e-4f;
    }
    const glm::vec3 eye{0.0f, 4.0f, 12.0f};
    const glm::vec3 forward = glm::normalize(glm::vec3{0.0f, 2.0f, 0.0f} - eye);
    const bool classificationValid =
        frustum.containsPoint({0.0f, 2.0f, 0.0f}) &&
        !frustum.containsPoint({0.0f, 2.0f, 30.0f}) &&
        !frustum.containsPoint({900.0f, 2.0f, 0.0f}) &&
        frustum.intersects({{0.0f, 2.0f, 0.0f}, 4.0f}) &&
        frustum.intersects({eye + forward * 0.4f, 1.0f});

    const glm::mat4 model =
        glm::scale(glm::translate(glm::mat4{1.0f}, {4.0f, 3.0f, -2.0f}),
                   {2.0f, 4.0f, 3.0f});
    const BoundingSphere transformed = transformedPrimitiveBounds(model, 0.8660254f);
    const bool boundsValid =
        glm::length(transformed.center - glm::vec3{4.0f, 3.0f, -2.0f}) < 1.0e-5f &&
        std::abs(transformed.radius - 4.6636895f) < 1.0e-4f;
    const bool valid = planesValid && classificationValid && boundsValid;

    output << "Phase 10 frustum validation\n"
           << "  six finite normalized planes: "
           << (planesValid ? "PASS" : "FAIL") << '\n'
           << "  inside/outside/boundary sphere tests: "
           << (classificationValid ? "PASS" : "FAIL") << '\n'
           << "  transformed conservative bounds: "
           << (boundsValid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Frustum checks passed.\n" : "Frustum checks failed.\n");
    return valid;
}
