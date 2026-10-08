#pragma once

#include <array>
#include <iosfwd>

#include <glm/glm.hpp>

#include "Shader.h"
#include "graphics/Mesh.h"
#include "lighting/SunController.h"

struct FireLight
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f, 0.55f, 0.24f};
    float intensity = 0.0f;
    float radius = 12.0f;
};

// Three fixed work-area fires. The same elapsed seconds own flame deformation,
// deterministic ember/smoke slots, and local-light flicker.
class FireSystem
{
public:
    static constexpr int count = 3;
    static constexpr int emberSlotsPerFire = 8;
    static constexpr int smokeSlotsPerFire = 4;

    FireSystem();
    void update(float dt);
    void seek(double seconds);
    void reset() { elapsedSeconds_ = 0.0; }
    double elapsedSeconds() const { return elapsedSeconds_; }
    std::array<FireLight, count> lights(const SunState& sun) const;
    std::array<glm::vec3, count> locations() const;
    void render(const SunState& sun, const glm::mat4& view,
                const glm::mat4& projection) const;

private:
    double elapsedSeconds_ = 0.0;
    Shader shader_;
    Mesh card_;
};

bool validateFireSystem(std::ostream& output);
