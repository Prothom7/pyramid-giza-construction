#pragma once

#include <iosfwd>

#include <glm/glm.hpp>

#include "lighting/SunController.h"

// World-space atmospheric perspective for the opaque scene. The target color
// is the existing sky horizon, so no second daylight palette or clock exists.
struct AtmosphericHaze
{
    static constexpr float startDistance = 80.0f;
    static constexpr float endDistance = 340.0f;
    static constexpr float maximumBlend = 0.92f;
    static constexpr float terrainEdgeWidthX = 200.0f;
    static constexpr float terrainEdgeWidthZ = 100.0f;
    static constexpr float terrainEdgeGateStart = 80.0f;
    static constexpr float terrainEdgeGateEnd = 130.0f;
    static constexpr float terrainEdgeMaximumBlend = 1.0f;

    static float factor(const glm::vec3& camera, const glm::vec3& fragment);
    static float terrainFactor(const glm::vec3& camera, const glm::vec3& fragment);
    static glm::vec3 color(const SunState& sun);
};

bool validateAtmosphericHaze(std::ostream& output);
