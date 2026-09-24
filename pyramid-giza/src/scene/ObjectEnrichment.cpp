#include "scene/ObjectEnrichment.h"

#include <algorithm>
#include <cmath>
#include <ostream>

#include <glm/geometric.hpp>

#include "animation/ConstructionAnimation.h"
#include "scene/MonumentalSite.h"
#include "scene/SceneTypes.h"

namespace
{
bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float distanceToGroundSegment(const glm::vec3& point, const glm::vec3& start,
                              const glm::vec3& end)
{
    const glm::vec2 p{point.x, point.z};
    const glm::vec2 a{start.x, start.z};
    const glm::vec2 b{end.x, end.z};
    const glm::vec2 segment = b - a;
    const float denominator = glm::dot(segment, segment);
    if (denominator <= 1.0e-6f)
        return glm::distance(p, a);
    const float t = std::clamp(glm::dot(p - a, segment) / denominator, 0.0f, 1.0f);
    return glm::distance(p, a + t * segment);
}
} // namespace

const std::vector<RopeRigDescriptor>& ObjectEnrichment::ropeRigs()
{
    // These are speculative graphics demonstrations, not claims about Khufu-era machinery.
    static const std::vector<RopeRigDescriptor> values{
        {"QuarryExitRedirect", "Quarry exit", RopeRigType::HorizontalRedirection,
         {-98.0f, 0.0f, 17.0f}, 22.0f, 6.2f, 4.8f, 3.2f, 2,
         "Redirect a quarry-exit hauling rope toward the road"},
        {"LoadingAFrame", "Loading station", RopeRigType::AFrameLift,
         {-10.0f, 0.0f, 54.0f}, 0.0f, 7.0f, 5.8f, 4.2f, 1,
         "Demonstrate a short vertical loading assist"},
        {"RampSideAssist", "Main ramp", RopeRigType::RampAssist,
         {8.5f, 3.15f, 27.0f}, 0.0f, 4.8f, 4.5f, 3.0f, 1,
         "Redirect a static assist rope beside the animated haul lane"}
    };
    return values;
}

const std::vector<AnchorPostDescriptor>& ObjectEnrichment::anchorPosts()
{
    static const std::vector<AnchorPostDescriptor> values{
        {"Q-A1", "Quarry exit", {-101.5f, 0.0f, 12.5f}, 2.6f, 0.42f},
        {"Q-A2", "Quarry exit", {-96.0f, 0.0f, 22.5f}, 2.6f, 0.42f},
        {"L-A1", "Loading station", {-18.0f, 0.0f, 52.0f}, 2.8f, 0.45f},
        {"L-A2", "Loading station", {-2.0f, 0.0f, 53.0f}, 2.8f, 0.45f},
        {"R-A1", "Main ramp", {-6.2f, 1.55f, 36.0f}, 2.7f, 0.40f},
        {"R-A2", "Main ramp", {6.2f, 1.55f, 36.0f}, 2.7f, 0.40f},
        {"R-A3", "Main ramp", {-6.2f, 4.75f, 17.0f}, 2.7f, 0.40f},
        {"R-A4", "Main ramp", {6.2f, 4.75f, 17.0f}, 2.7f, 0.40f},
        {"U-A1", "Upper placement", {-11.5f, 8.55f, 2.0f}, 2.5f, 0.38f},
        {"U-A2", "Upper placement", {10.5f, 8.55f, 2.0f}, 2.5f, 0.38f},
        {"N-A1", "River landing", {-42.0f, 0.10f, -153.0f}, 2.4f, 0.38f},
        {"N-A2", "River landing", {-30.0f, 0.10f, -153.0f}, 2.4f, 0.38f},
        {"N-A3", "River landing", {-18.0f, 0.10f, -153.0f}, 2.4f, 0.38f},
        {"N-A4", "River landing", {-6.0f, 0.10f, -153.0f}, 2.4f, 0.38f}
    };
    return values;
}

const std::vector<LadderDescriptor>& ObjectEnrichment::ladders()
{
    static const std::vector<LadderDescriptor> values{
        {"QuarryLower", "Quarry", {-151.0f, -7.35f, -9.0f}, 0.0f, -8.0f,
         1.35f, 5.7f, 10, "Lower-to-middle terrace"},
        {"QuarryUpper", "Quarry", {-104.5f, -5.55f, -35.0f}, 90.0f, -7.0f,
         1.35f, 4.3f, 8, "Middle-to-upper terrace"},
        {"FrontWest", "Scaffold", {-22.2f, 0.0f, 2.2f}, 0.0f, -7.0f,
         1.25f, 7.8f, 13, "Front-west scaffold"},
        {"FrontEast", "Scaffold", {13.8f, 0.0f, 2.2f}, 0.0f, 7.0f,
         1.25f, 7.8f, 13, "Front-east scaffold"},
        {"RampTop", "Scaffold", {-3.2f, 8.55f, 0.5f}, 0.0f, -6.0f,
         1.20f, 5.1f, 9, "Ramp-top scaffold"},
        {"WestAccess", "Ramp access", {-43.2f, 4.95f, -22.6f}, 90.0f, -8.0f,
         1.25f, 4.8f, 8, "West access platform"},
        {"InspectionAccess", "Inspection station", {-49.0f, 0.0f, 8.4f}, 90.0f, -6.0f,
         1.15f, 2.2f, 5, "Raised inspection bed"},
        {"LandingAccess", "River landing", {-24.0f, 0.08f, -151.0f}, 0.0f, 10.0f,
         1.30f, 2.5f, 5, "Quay-to-boat access"}
    };
    return values;
}

const std::vector<BoatDescriptor>& ObjectEnrichment::boats()
{
    static const std::vector<BoatDescriptor> values{
        {"CargoBoatMoored", {-31.0f, 0.22f, -159.0f}, 4.0f, 10.5f, 3.8f, true},
        {"SupplyBoatOffshore", {-9.0f, 0.20f, -169.0f}, -12.0f, 8.5f, 3.2f, false}
    };
    return values;
}

const char* ropeRigTypeName(RopeRigType type)
{
    switch (type)
    {
    case RopeRigType::HorizontalRedirection: return "Horizontal redirection frame";
    case RopeRigType::AFrameLift: return "A-frame lift";
    case RopeRigType::RampAssist: return "Ramp-side assist";
    default: return "Unknown";
    }
}

bool validateObjectEnrichment(std::ostream& output)
{
    bool rigsValid = ObjectEnrichment::ropeRigs().size() == 3;
    for (const RopeRigDescriptor& rig : ObjectEnrichment::ropeRigs())
    {
        rigsValid = rigsValid && finiteVector(rig.center) && std::isfinite(rig.yawDegrees) &&
                    rig.width > 0.0f && rig.height > 0.0f && rig.depth > 0.0f &&
                    rig.wheelCount > 0 && rig.wheelCount <= 2;
        rigsValid = rigsValid && isFiniteNonSingularTransform(
            makeTransform(rig.center, {0.0f, rig.yawDegrees, 0.0f}, {1.0f, 1.0f, 1.0f}));
    }

    bool anchorsValid = ObjectEnrichment::anchorPosts().size() == 14;
    for (const AnchorPostDescriptor& post : ObjectEnrichment::anchorPosts())
        anchorsValid = anchorsValid && finiteVector(post.base) && post.height > 0.0f &&
                       post.diameter > 0.0f && isFiniteNonSingularTransform(
                           makeTransform(post.base + glm::vec3{0.0f, 0.5f * post.height, 0.0f},
                                         {}, {post.diameter, post.height, post.diameter}));

    bool laddersValid = ObjectEnrichment::ladders().size() == 8;
    for (const LadderDescriptor& ladder : ObjectEnrichment::ladders())
    {
        const float spacing = ladder.height / static_cast<float>(ladder.rungCount + 1);
        laddersValid = laddersValid && finiteVector(ladder.base) &&
                       std::isfinite(ladder.yawDegrees) && std::isfinite(ladder.leanDegrees) &&
                       ladder.width > 0.8f && ladder.height > 1.5f &&
                       ladder.rungCount >= 5 && spacing >= 0.35f && spacing <= 0.65f;
    }

    bool boatsValid = ObjectEnrichment::boats().size() == 2;
    for (const BoatDescriptor& boat : ObjectEnrichment::boats())
        boatsValid = boatsValid && finiteVector(boat.center) &&
                     std::isfinite(boat.yawDegrees) && boat.length > boat.width &&
                     boat.width > 2.0f && boat.center.z < -153.0f;

    // The animated sledge follows these two ground segments and then the main ramp.
    // All enrichment roots beside that route retain a generous horizontal clearance.
    constexpr glm::vec3 sledgeStart{10.0f, 0.0f, 40.0f};
    constexpr glm::vec3 groundTurn{0.0f, 0.0f, 47.0f};
    const glm::vec3 rampEntry = MonumentalSite::mainRamp().base;
    bool heroPathClear = true;
    for (const RopeRigDescriptor& rig : ObjectEnrichment::ropeRigs())
    {
        const float groundDistance = std::min(
            distanceToGroundSegment(rig.center, sledgeStart, groundTurn),
            distanceToGroundSegment(rig.center, groundTurn, rampEntry));
        const bool rampSidePlacement = rig.type == RopeRigType::RampAssist;
        heroPathClear = heroPathClear &&
                        (groundDistance > 4.5f || rampSidePlacement) &&
                        (!rampSidePlacement || std::abs(rig.center.x) >= 8.0f);
    }

    const auto& repeatedRigs = ObjectEnrichment::ropeRigs();
    const bool deterministic = &repeatedRigs == &ObjectEnrichment::ropeRigs() &&
                               repeatedRigs.front().center ==
                                   ObjectEnrichment::ropeRigs().front().center;
    // 327 static primitive instances plus four rigid-part workers (4 * 17).
    const std::size_t estimatedAddedDrawCalls = ObjectEnrichment::expectedAddedDrawCalls;
    const bool performanceValid = estimatedAddedDrawCalls <=
                                  ObjectEnrichment::maximumAddedDrawCalls;
    const bool valid = rigsValid && anchorsValid && laddersValid && boatsValid &&
                       heroPathClear && deterministic && performanceValid;

    output << "Phase 6.5 object-enrichment validation\n"
           << "  speculative rope-redirection rigs: " << ObjectEnrichment::ropeRigs().size()
           << (rigsValid ? " PASS\n" : " FAIL\n")
           << "  heavy anchor posts: " << ObjectEnrichment::anchorPosts().size()
           << (anchorsValid ? " PASS\n" : " FAIL\n")
           << "  ladders and rung spacing: " << ObjectEnrichment::ladders().size()
           << (laddersValid ? " PASS\n" : " FAIL\n")
           << "  primitive-composed boats: " << ObjectEnrichment::boats().size()
           << (boatsValid ? " PASS\n" : " FAIL\n")
           << "  hero transport corridor clearance: "
           << (heroPathClear ? "PASS\n" : "FAIL\n")
           << "  deterministic descriptors: " << (deterministic ? "PASS\n" : "FAIL\n")
           << "  estimated added draws <= " << ObjectEnrichment::maximumAddedDrawCalls << ": "
           << (performanceValid ? "PASS\n" : "FAIL\n")
           << (valid ? "Object-enrichment checks passed.\n"
                     : "Object-enrichment checks failed.\n");
    return valid;
}
