#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"
#include "lighting/SunController.h"

struct SkyColors
{
    glm::vec3 horizon{0.0f};
    glm::vec3 zenith{0.0f};
};

// The sun state is the sole time-of-day authority; the camera supplies only direction.
SkyColors skyColors(const SunState& sun);
glm::vec3 skyViewDirection(const glm::mat4& view, const glm::mat4& projection,
                           const glm::vec2& ndc);
glm::vec3 skyColorAt(const SkyColors& colors, const glm::vec3& worldDirection);
bool validateSkyBackground(std::ostream& output);

class SkyBackground
{
public:
    SkyBackground();
    void render(const SunState& sun, const glm::mat4& view,
                const glm::mat4& projection) const;

private:
    Shader shader_;
    Mesh quad_;
};
