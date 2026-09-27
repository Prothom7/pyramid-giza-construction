#include "scene/RampValidation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/ConstructionTimeline.h"
#include "objects/Scaffold.h"
#include "scene/MonumentalSite.h"
#include "scene/PyramidLayout.h"
#include "scene/SupportSystem.h"

namespace
{
struct Obb
{
    glm::vec3 center{0.0f};
    std::array<glm::vec3, 3> axis{{{1.0f, 0.0f, 0.0f},
                                   {0.0f, 1.0f, 0.0f},
                                   {0.0f, 0.0f, 1.0f}}};
    glm::vec3 half{0.5f};
};

Obb rampBox(const RampDescriptor& ramp, float shrink = 0.0f)
{
    const RampFrame frame = MonumentalSite::rampFrame(ramp);
    return {0.5f * (ramp.base + ramp.top),
            {frame.right, frame.up, frame.forward},
            {std::max(0.01f, 0.5f * ramp.width - shrink),
             0.5f * ramp.thickness,
             0.5f * frame.length}};
}

Obb blockBox(const PyramidBlockPlacement& block)
{
    return {block.position,
            {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 1.0f}}},
            0.5f * block.scale};
}

Obb scaffoldBox(const ScaffoldPlacement& scaffold)
{
    const float yaw = glm::radians(scaffold.rotationY);
    const glm::vec3 xAxis{std::cos(yaw), 0.0f, -std::sin(yaw)};
    const glm::vec3 zAxis{std::sin(yaw), 0.0f, std::cos(yaw)};
    const glm::vec3 localCenter{
        0.5f * static_cast<float>(scaffold.bays - 1u) * Scaffold::width(),
        0.5f * static_cast<float>(scaffold.levels) * Scaffold::levelHeight(),
        0.0f};
    return {scaffold.origin + xAxis * localCenter.x +
                                  glm::vec3{0.0f, localCenter.y, 0.0f},
            {xAxis, {0.0f, 1.0f, 0.0f}, zAxis},
            {0.5f * static_cast<float>(scaffold.bays) * Scaffold::width() + 0.2f,
             0.5f * static_cast<float>(scaffold.levels) * Scaffold::levelHeight() + 0.1f,
             0.5f * Scaffold::depth() + 0.2f}};
}

bool overlaps(const Obb& a, const Obb& b, float& minimumPenetration)
{
    constexpr float epsilon = 1.0e-5f;
    float rotation[3][3]{};
    float absoluteRotation[3][3]{};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
        {
            rotation[row][column] = glm::dot(a.axis[row], b.axis[column]);
            absoluteRotation[row][column] =
                std::abs(rotation[row][column]) + epsilon;
        }

    const glm::vec3 delta = b.center - a.center;
    const glm::vec3 translated{glm::dot(delta, a.axis[0]),
                               glm::dot(delta, a.axis[1]),
                               glm::dot(delta, a.axis[2])};
    minimumPenetration = std::numeric_limits<float>::max();
    const auto separated = [&minimumPenetration](float distance, float radius) {
        const float penetration = radius - std::abs(distance);
        minimumPenetration = std::min(minimumPenetration, penetration);
        return penetration < 0.0f;
    };

    for (int row = 0; row < 3; ++row)
    {
        const float bRadius = b.half.x * absoluteRotation[row][0] +
                              b.half.y * absoluteRotation[row][1] +
                              b.half.z * absoluteRotation[row][2];
        if (separated(translated[row], a.half[row] + bRadius))
            return false;
    }
    for (int column = 0; column < 3; ++column)
    {
        const float distance = translated.x * rotation[0][column] +
                               translated.y * rotation[1][column] +
                               translated.z * rotation[2][column];
        const float aRadius = a.half.x * absoluteRotation[0][column] +
                              a.half.y * absoluteRotation[1][column] +
                              a.half.z * absoluteRotation[2][column];
        if (separated(distance, aRadius + b.half[column]))
            return false;
    }

    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
        {
            const int nextRow = (row + 1) % 3;
            const int lastRow = (row + 2) % 3;
            const int nextColumn = (column + 1) % 3;
            const int lastColumn = (column + 2) % 3;
            const float distance =
                translated[lastRow] * rotation[nextRow][column] -
                translated[nextRow] * rotation[lastRow][column];
            const float aRadius =
                a.half[nextRow] * absoluteRotation[lastRow][column] +
                a.half[lastRow] * absoluteRotation[nextRow][column];
            const float bRadius =
                b.half[nextColumn] * absoluteRotation[row][lastColumn] +
                b.half[lastColumn] * absoluteRotation[row][nextColumn];
            if (separated(distance, aRadius + bRadius))
                return false;
        }
    return true;
}

bool active(float progress, float minimum, float maximum)
{
    return progress >= minimum &&
           (progress < maximum || (progress >= 1.0f && maximum > 1.0f));
}

bool intendedJunction(const RampDescriptor& a, const RampDescriptor& b)
{
    const bool matchingStage = std::abs(a.minimumProgress - b.minimumProgress) < 1.0e-5f &&
                               std::abs(a.maximumProgress - b.maximumProgress) < 1.0e-5f;
    const float closest = std::min({glm::distance(a.base, b.base),
                                    glm::distance(a.base, b.top),
                                    glm::distance(a.top, b.base),
                                    glm::distance(a.top, b.top)});
    return matchingStage && closest < 1.25f;
}
} // namespace

bool validatePhase12_5RampClearance(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> blocks =
        PyramidLayout::generateComplete(config);
    const std::array<float, 5> checkpoints{{0.0f, 0.25f, 0.50f, 0.75f, 1.0f}};
    bool valid = true;

    output << "Phase 12.5 ramp-clearance validation\n";
    for (float progress : checkpoints)
    {
        ConstructionTimelineController timeline;
        timeline.setProgress(progress);
        std::size_t activeRamps = 0;
        std::size_t activeScaffolds = 0;
        std::size_t visibleBlocks = 0;
        std::size_t unintendedPyramid = 0;
        std::size_t intendedPyramid = 0;
        std::size_t unintendedRamp = 0;
        std::size_t intendedRamp = 0;
        std::size_t scaffoldConflicts = 0;
        std::size_t corridorObstructions = 0;
        std::size_t unsupported = 0;
        float maximumPenetration = 0.0f;

        for (const PyramidBlockPlacement& block : blocks)
            if (timeline.blockState(block, config).visible)
                ++visibleBlocks;

        const auto& ramps = MonumentalSite::ramps();
        for (std::size_t rampIndex = 0; rampIndex < ramps.size(); ++rampIndex)
        {
            const RampDescriptor& ramp = ramps[rampIndex];
            if (!MonumentalSite::rampActive(ramp, progress))
                continue;
            ++activeRamps;
            const Obb rampObb = rampBox(ramp, 0.04f);
            for (float sample : {0.0f, 0.25f, 0.50f, 0.75f, 1.0f})
            {
                const SupportSurface support =
                    SceneSupport::rampFillAt(ramp, sample);
                const float gap =
                    SceneSupport::rampUndersideY(ramp, sample) -
                    support.height;
                if (!std::isfinite(gap) ||
                    gap < SceneSupport::minimumContactGap ||
                    gap > SceneSupport::maximumContactGap)
                    ++unsupported;
            }
            if (ramp.animatedRoute &&
                (ramp.width < 5.0f || ramp.sideClearance < 0.60f))
                ++corridorObstructions;

            std::size_t rampPyramidContacts = 0;
            for (const PyramidBlockPlacement& block : blocks)
            {
                if (!timeline.blockState(block, config).visible)
                    continue;
                float penetration = 0.0f;
                if (!overlaps(rampObb, blockBox(block), penetration))
                    continue;
                maximumPenetration = std::max(maximumPenetration, penetration);
                if (ramp.intentionalPyramidContact)
                    ++intendedPyramid;
                else
                    ++unintendedPyramid;
                ++rampPyramidContacts;
            }
            if (rampPyramidContacts > 0u)
                output << "    contact " << ramp.id << " -> pyramid blocks: "
                       << rampPyramidContacts << '\n';

            for (std::size_t other = rampIndex + 1; other < ramps.size(); ++other)
            {
                if (!MonumentalSite::rampActive(ramps[other], progress))
                    continue;
                float penetration = 0.0f;
                if (!overlaps(rampObb, rampBox(ramps[other], 0.04f), penetration))
                    continue;
                maximumPenetration = std::max(maximumPenetration, penetration);
                if (intendedJunction(ramp, ramps[other]))
                    ++intendedRamp;
                else
                    ++unintendedRamp;
            }

            for (const ScaffoldPlacement& scaffold : MonumentalSite::scaffolds())
            {
                if (!active(progress, scaffold.minimumProgress,
                            scaffold.maximumProgress))
                    continue;
                float penetration = 0.0f;
                if (overlaps(rampObb, scaffoldBox(scaffold), penetration))
                {
                    ++scaffoldConflicts;
                    output << "    contact " << ramp.id << " -> scaffold "
                           << scaffold.id << '\n';
                    maximumPenetration = std::max(maximumPenetration, penetration);
                }
            }
        }

        for (const ScaffoldPlacement& scaffold : MonumentalSite::scaffolds())
            if (active(progress, scaffold.minimumProgress,
                       scaffold.maximumProgress))
                ++activeScaffolds;

        const bool checkpointValid = unintendedPyramid == 0u &&
                                     unintendedRamp == 0u &&
                                     scaffoldConflicts == 0u &&
                                     corridorObstructions == 0u &&
                                     unsupported == 0u;
        valid = valid && checkpointValid;
        output << std::fixed << std::setprecision(2)
               << "  progress " << progress
               << ": visibleBlocks=" << visibleBlocks
               << ", ramps=" << activeRamps
               << ", scaffolds=" << activeScaffolds
               << ", pyramidUnexpected=" << unintendedPyramid
               << ", pyramidAllowed=" << intendedPyramid
               << ", rampUnexpected=" << unintendedRamp
               << ", rampJunctions=" << intendedRamp
               << ", scaffoldConflicts=" << scaffoldConflicts
               << ", corridorObstructions=" << corridorObstructions
               << ", unsupportedSamples=" << unsupported
               << ", maxPenetration=" << maximumPenetration
               << (checkpointValid ? " PASS\n" : " FAIL\n");
    }
    output << (valid ? "Ramp-clearance checks passed.\n"
                     : "Ramp-clearance checks failed.\n");
    return valid;
}
