#pragma once

#include <array>
#include <cstddef>
#include <iosfwd>

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"
#include "lighting/SunController.h"

struct CloudDescriptor
{
    glm::vec3 center;
    glm::vec2 size;
    float speed;
    float opacity;
    float seed;
};

// Fixed world-space cloud layout. Only elapsed simulation time owns drift;
// SunState owns daylight tint independently.
class CloudField
{
public:
    static constexpr std::size_t count = 8;
    static constexpr double westEdge = -1000.0;
    static constexpr double eastEdge = 1000.0;

    void update(float dt);
    void seek(double seconds);
    void reset() { elapsedSeconds_ = 0.0; }
    double elapsedSeconds() const { return elapsedSeconds_; }
    std::array<CloudDescriptor, count> clouds() const;
    static glm::vec3 tint(const SunState& sun);

private:
    double elapsedSeconds_ = 0.0;
};

bool validateCloudLayer(std::ostream& output);

class CloudLayer
{
public:
    CloudLayer();
    void update(float dt) { field_.update(dt); }
    void seek(double seconds) { field_.seek(seconds); }
    void reset() { field_.reset(); }
    void render(const SunState& sun, const glm::mat4& view,
                const glm::mat4& projection, const glm::vec3& cameraPosition) const;

private:
    CloudField field_;
    Shader shader_;
    Mesh card_;
};
