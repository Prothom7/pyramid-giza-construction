#include "scene/SceneIntegrity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

#include "animation/ConstructionAnimation.h"
#include "camera/CameraController.h"
#include "lighting/ShadowMap.h"
#include "scene/MonumentalSite.h"

namespace
{
bool finite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool contains(const SceneBounds& outer, const SceneBounds& inner)
{
    return glm::all(glm::lessThanEqual(outer.minimum, inner.minimum)) &&
           glm::all(glm::greaterThanEqual(outer.maximum, inner.maximum));
}

bool overlapsHorizontal(const SiteZoneDescriptor& a, const SiteZoneDescriptor& b)
{
    return std::abs(a.center.x - b.center.x) < a.extents.x + b.extents.x &&
           std::abs(a.center.z - b.center.z) < a.extents.z + b.extents.z;
}

bool intentionalOverlap(const std::string& first, const std::string& second)
{
    const auto pairMatches = [&](const char* a, const char* b)
    {
        return (first == a && second == b) || (first == b && second == a);
    };
    return pairMatches("PyramidZone", "RampNetwork") ||
           pairMatches("PyramidZone", "ScaffoldZone") ||
           pairMatches("RampNetwork", "ScaffoldZone") ||
           pairMatches("RampNetwork", "LoadingStation") ||
           pairMatches("RampNetwork", "TransportZone") ||
           pairMatches("QuarryZone", "RoughRepository") ||
           pairMatches("PyramidZone", "TimberYard") ||
           pairMatches("PyramidZone", "WorkCamp") ||
           pairMatches("RampNetwork", "FinishedRepository") ||
           pairMatches("RoughRepository", "CuttingZone") ||
           pairMatches("CuttingZone", "FinishedRepository") ||
           pairMatches("FinishedRepository", "LoadingStation") ||
           pairMatches("RoughRepository", "TransportZone") ||
           pairMatches("CuttingZone", "TransportZone") ||
           pairMatches("FinishedRepository", "TransportZone") ||
           pairMatches("LoadingStation", "TransportZone");
}
}

SceneBounds SceneIntegrity::contentBounds()
{
    SceneBounds bounds;
    bounds.minimum = glm::vec3{std::numeric_limits<float>::max()};
    bounds.maximum = glm::vec3{std::numeric_limits<float>::lowest()};
    for (const SiteZoneDescriptor& zone : MonumentalSite::zones())
    {
        bounds.minimum = glm::min(bounds.minimum, zone.center - zone.extents);
        bounds.maximum = glm::max(bounds.maximum, zone.center + zone.extents);
    }
    return bounds;
}

SceneBounds SceneIntegrity::finalGroundBounds()
{
    return {{-213.0f, -0.62f, -213.0f}, {213.0f, 0.0f, 123.0f}};
}

glm::vec4 SceneIntegrity::horizontalSafetyMargins()
{
    const SceneBounds content = contentBounds();
    const SceneBounds ground = finalGroundBounds();
    return {content.minimum.x - ground.minimum.x,
            ground.maximum.x - content.maximum.x,
            content.minimum.z - ground.minimum.z,
            ground.maximum.z - content.maximum.z};
}

bool validateSceneIntegrity(std::ostream& output)
{
    const SceneBounds content = SceneIntegrity::contentBounds();
    const SceneBounds ground = SceneIntegrity::finalGroundBounds();
    const glm::vec4 margins = SceneIntegrity::horizontalSafetyMargins();
    bool valid = finite(content.minimum) && finite(content.maximum) &&
                 finite(ground.minimum) && finite(ground.maximum);
    SceneBounds horizontalContent = content;
    horizontalContent.minimum.y = ground.minimum.y;
    horizontalContent.maximum.y = ground.maximum.y;
    valid = valid && contains(ground, horizontalContent);
    valid = valid && margins.x >= 30.0f && margins.y >= 30.0f &&
            margins.z >= 30.0f && margins.w >= 30.0f;

    std::size_t intentional = 0;
    std::size_t unexpected = 0;
    const auto& zones = MonumentalSite::zones();
    for (std::size_t a = 0; a < zones.size(); ++a)
    {
        for (std::size_t b = a + 1; b < zones.size(); ++b)
        {
            if (!overlapsHorizontal(zones[a], zones[b]))
                continue;
            if (intentionalOverlap(zones[a].id, zones[b].id))
                ++intentional;
            else
                ++unexpected;
        }
    }
    valid = valid && intentional >= 6 && unexpected == 0;

    const ShadowSettings shadow;
    valid = valid &&
            content.minimum.x >= shadow.center.x - shadow.halfWidth &&
            content.maximum.x <= shadow.center.x + shadow.halfWidth &&
            content.minimum.z >= shadow.center.z - shadow.halfHeight &&
            content.maximum.z <= shadow.center.z + shadow.halfHeight;

    const float horizontalDiagonal = glm::length(glm::vec2{
        ground.maximum.x - ground.minimum.x,
        ground.maximum.z - ground.minimum.z});
    valid = valid && CameraController::farPlane > horizontalDiagonal;

    glm::vec3 previous =
        ConstructionAnimationController::transportAt(0.0f).sledgePosition;
    for (int sample = 1; sample <= 100; ++sample)
    {
        const glm::vec3 current =
            ConstructionAnimationController::transportAt(sample / 100.0f).sledgePosition;
        valid = valid && finite(current) && glm::distance(previous, current) < 3.0f &&
                current.x > ground.minimum.x && current.x < ground.maximum.x &&
                current.z > ground.minimum.z && current.z < ground.maximum.z;
        previous = current;
    }

    output << "Phase 9 scene-integrity validation\n"
           << "  content AABB: (" << content.minimum.x << ", " << content.minimum.y
           << ", " << content.minimum.z << ") to (" << content.maximum.x << ", "
           << content.maximum.y << ", " << content.maximum.z << ")\n"
           << "  final ground AABB: (" << ground.minimum.x << ", " << ground.minimum.z
           << ") to (" << ground.maximum.x << ", " << ground.maximum.z << ")\n"
           << "  margins left/right/back/front: " << margins.x << ", " << margins.y
           << ", " << margins.z << ", " << margins.w << '\n'
           << "  intentional overlaps: " << intentional
           << "; unexpected overlaps: " << unexpected << '\n'
           << "  camera far plane / ground diagonal: " << CameraController::farPlane
           << " / " << horizontalDiagonal << '\n'
           << "  transport corridor: sampled and clear inside ground coverage\n"
           << (valid ? "Scene-integrity checks passed.\n"
                     : "Scene-integrity checks failed.\n");
    return valid;
}
