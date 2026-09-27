#include "scene/PyramidInterior.h"

#include "animation/ConstructionTimeline.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>

namespace
{
struct PassageFrame
{
    glm::vec3 center{0.0f};
    glm::vec3 forward{0.0f};
    glm::vec3 right{0.0f};
    glm::vec3 up{0.0f};
    float length = 0.0f;
};

PassageFrame frameFor(const PassageSegment& passage)
{
    const glm::vec3 delta = passage.end - passage.start;
    const float length = glm::length(delta);
    const glm::vec3 forward = delta / length;
    glm::vec3 right = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, forward);
    if (glm::length(right) < 1.0e-5f)
        right = {1.0f, 0.0f, 0.0f};
    right = glm::normalize(right);
    const glm::vec3 up = glm::normalize(glm::cross(forward, right));
    return {0.5f * (passage.start + passage.end) +
                up * (0.5f * passage.height),
            forward, right, up, length};
}

glm::mat4 orientedBox(const glm::vec3& center, const glm::vec3& right,
                      const glm::vec3& up, const glm::vec3& forward,
                      const glm::vec3& dimensions)
{
    glm::mat4 model{1.0f};
    model[0] = glm::vec4{right * dimensions.x, 0.0f};
    model[1] = glm::vec4{up * dimensions.y, 0.0f};
    model[2] = glm::vec4{forward * dimensions.z, 0.0f};
    model[3] = glm::vec4{center, 1.0f};
    return model;
}

bool finite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool intersectsPassage(const PyramidBlockPlacement& block,
                       const PassageSegment& passage)
{
    const PassageFrame frame = frameFor(passage);
    const glm::vec3 delta = block.position - frame.center;
    const glm::vec3 blockHalf = 0.5f * block.scale;
    const auto overlaps = [&](const glm::vec3& axis, float halfExtent) {
        const float projectedRadius = glm::dot(glm::abs(axis), blockHalf);
        return std::abs(glm::dot(delta, axis)) <=
               halfExtent + passage.carvingMargin + projectedRadius;
    };
    return overlaps(frame.right, 0.5f * passage.width) &&
           overlaps(frame.up, 0.5f * passage.height) &&
           overlaps(frame.forward, 0.5f * frame.length);
}

bool intersectsRoom(const PyramidBlockPlacement& block,
                    const InteriorRoom& room)
{
    const glm::vec3 combined = room.halfExtents +
                               glm::vec3{room.carvingMargin} +
                               0.5f * block.scale;
    const glm::vec3 delta = glm::abs(block.position - room.center);
    return delta.x <= combined.x && delta.y <= combined.y &&
           delta.z <= combined.z;
}

bool blockContainsPoint(const PyramidBlockPlacement& block,
                        const glm::vec3& point)
{
    const glm::vec3 delta = glm::abs(point - block.position);
    const glm::vec3 half = 0.5f * block.scale;
    return delta.x <= half.x && delta.y <= half.y && delta.z <= half.z;
}

bool passageCameraPosition(const PassageSegment& passage,
                           const glm::vec3& candidate,
                           glm::vec3& constrained)
{
    const glm::vec2 horizontalDelta{passage.end.x - passage.start.x,
                                    passage.end.z - passage.start.z};
    const float horizontalLengthSquared = glm::dot(horizontalDelta,
                                                    horizontalDelta);
    if (horizontalLengthSquared <= 1.0e-6f)
        return false;
    const glm::vec2 fromStart{candidate.x - passage.start.x,
                              candidate.z - passage.start.z};
    const float t = glm::dot(fromStart, horizontalDelta) /
                    horizontalLengthSquared;
    if (t < -0.035f || t > 1.035f)
        return false;
    const float clampedT = std::clamp(t, 0.0f, 1.0f);
    const glm::vec2 center = glm::mix(glm::vec2{passage.start.x, passage.start.z},
                                      glm::vec2{passage.end.x, passage.end.z},
                                      clampedT);
    const glm::vec2 horizontalForward = glm::normalize(horizontalDelta);
    const glm::vec2 horizontalRight{horizontalForward.y, -horizontalForward.x};
    const float lateral = glm::dot(glm::vec2{candidate.x, candidate.z} - center,
                                   horizontalRight);
    if (std::abs(lateral) > 0.5f * passage.width - 0.28f)
        return false;
    const float floorY = glm::mix(passage.start.y, passage.end.y, clampedT);
    constrained = {candidate.x, floorY + PyramidInterior::eyeHeight, candidate.z};
    return true;
}

bool roomCameraPosition(const InteriorRoom& room, const glm::vec3& candidate,
                        glm::vec3& constrained)
{
    const float margin = 0.45f;
    if (std::abs(candidate.x - room.center.x) > room.halfExtents.x - margin ||
        std::abs(candidate.z - room.center.z) > room.halfExtents.z - margin)
        return false;
    const float floorY = room.center.y - room.halfExtents.y;
    constrained = {candidate.x, floorY + PyramidInterior::eyeHeight, candidate.z};
    return constrained.y < room.center.y + room.halfExtents.y - 0.30f;
}

float pyramidHalfExtentAt(float y)
{
    constexpr float baseHalfExtent = 40.82f;
    constexpr float inwardPerWorldY = 1.46f / 2.0f;
    return baseHalfExtent - std::max(0.0f, y) * inwardPerWorldY;
}
} // namespace

const std::array<PassageSegment, 4>& PyramidInterior::passages()
{
    static const std::array<PassageSegment, 4> values{{
        {"EntryDescending", InteriorSpaceType::Passage,
         {0.0f, 7.0f, -83.0f}, {0.0f, 4.5f, -67.0f},
         2.2f, 2.6f, 0.24f, "North entrance and descending entry"},
        {"AscendingPassage", InteriorSpaceType::Passage,
         {0.0f, 4.5f, -67.0f}, {0.0f, 11.0f, -55.0f},
         2.2f, 2.7f, 0.24f, "Inclined transition toward gallery"},
        {"GrandGallery", InteriorSpaceType::Gallery,
         {0.0f, 11.0f, -55.0f}, {0.0f, 16.0f, -43.0f},
         3.6f, 6.0f, 0.24f, "Taller gallery-like corridor"},
        {"AntechamberConnector", InteriorSpaceType::Passage,
         {0.0f, 16.0f, -43.0f}, {0.0f, 16.0f, -39.5f},
         3.0f, 3.5f, 0.24f, "Level doorway into antechamber"}
    }};
    return values;
}

const std::array<InteriorRoom, 2>& PyramidInterior::rooms()
{
    static const std::array<InteriorRoom, 2> values{{
        {"Antechamber", InteriorSpaceType::Antechamber,
         {0.0f, 18.0f, -38.0f}, {2.5f, 2.0f, 2.8f}, 0.24f,
         "Transition room before tomb"},
        {"TombChamber", InteriorSpaceType::TombChamber,
         {0.0f, 19.0f, -29.5f}, {5.0f, 3.0f, 7.5f}, 0.24f,
         "Main burial chamber and sarcophagus"}
    }};
    return values;
}

const std::vector<InteriorPart>& PyramidInterior::architecturalParts()
{
    static const std::vector<InteriorPart> values = [] {
        std::vector<InteriorPart> result;
        result.reserve(40);
        constexpr float slab = 0.14f;
        const std::array<float, 4> stages{{0.18f, 0.30f, 0.48f, 0.58f}};
        for (std::size_t index = 0; index < passages().size(); ++index)
        {
            const PassageSegment& passage = passages()[index];
            const PassageFrame frame = frameFor(passage);
            const float stage = stages[index];
            result.push_back({"PassageFloor",
                orientedBox(frame.center - frame.up * (0.5f * passage.height +
                                                       0.5f * slab),
                            frame.right, frame.up, frame.forward,
                            {passage.width, slab, frame.length}),
                MaterialId::Limestone, stage});
            result.push_back({"PassageCeiling",
                orientedBox(frame.center + frame.up * (0.5f * passage.height +
                                                       0.5f * slab),
                            frame.right, frame.up, frame.forward,
                            {passage.width, slab, frame.length}),
                MaterialId::QuarryStone, stage + 0.05f});
            for (float sign : {-1.0f, 1.0f})
                result.push_back({"PassageWall",
                    orientedBox(frame.center + frame.right * sign *
                                    (0.5f * passage.width + 0.5f * slab),
                                frame.right, frame.up, frame.forward,
                                {slab, passage.height, frame.length}),
                    MaterialId::LimestoneVariation, stage});
        }

        const auto addRoomShell = [&](const InteriorRoom& room, float stage,
                                      bool backWall) {
            const glm::vec3 size = 2.0f * room.halfExtents;
            result.push_back({"RoomFloor",
                makeTransform({room.center.x,
                               room.center.y - room.halfExtents.y - 0.5f * slab,
                               room.center.z}, {}, {size.x, slab, size.z}),
                MaterialId::Limestone, stage});
            result.push_back({"RoomCeiling",
                makeTransform({room.center.x,
                               room.center.y + room.halfExtents.y + 0.5f * slab,
                               room.center.z}, {}, {size.x, slab, size.z}),
                MaterialId::QuarryStone, stage + 0.05f});
            for (float sign : {-1.0f, 1.0f})
                result.push_back({"RoomSideWall",
                    makeTransform({room.center.x + sign *
                                                       (room.halfExtents.x +
                                                        0.5f * slab),
                                   room.center.y, room.center.z}, {},
                                  {slab, size.y, size.z}),
                    MaterialId::LimestoneVariation, stage});
            if (backWall)
                result.push_back({"RoomBackWall",
                    makeTransform({room.center.x, room.center.y,
                                   room.center.z + room.halfExtents.z +
                                       0.5f * slab}, {},
                                  {size.x, size.y, slab}),
                    MaterialId::LimestoneVariation, stage});
        };
        addRoomShell(rooms()[0], 0.62f, false);
        addRoomShell(rooms()[1], 0.70f, true);

        // A simple lintel and jamb pair makes the one intentional exterior
        // opening readable without covering the carved block opening.
        result.push_back({"EntranceLeftJamb",
            makeTransform({-1.35f, 8.30f, -81.30f}, {}, {0.35f, 3.0f, 0.50f}),
            MaterialId::PreparedStone, 0.20f});
        result.push_back({"EntranceRightJamb",
            makeTransform({1.35f, 8.30f, -81.30f}, {}, {0.35f, 3.0f, 0.50f}),
            MaterialId::PreparedStone, 0.20f});
        result.push_back({"EntranceLintel",
            makeTransform({0.0f, 9.88f, -81.30f}, {}, {3.05f, 0.35f, 0.50f}),
            MaterialId::PreparedStone, 0.20f});

        // Open stone sarcophagus: base plus four walls, all supported by the
        // tomb floor at y=16.0.
        result.push_back({"SarcophagusBase",
            makeTransform({0.0f, 16.15f, -27.0f}, {}, {2.8f, 0.30f, 5.0f}),
            MaterialId::PreparedStone, 0.72f});
        for (float sign : {-1.0f, 1.0f})
            result.push_back({"SarcophagusSide",
                makeTransform({sign * 1.25f, 16.80f, -27.0f}, {},
                              {0.30f, 1.30f, 5.0f}),
                MaterialId::PreparedStone, 0.72f});
        for (float sign : {-1.0f, 1.0f})
            result.push_back({"SarcophagusEnd",
                makeTransform({0.0f, 16.80f, -27.0f + sign * 2.35f}, {},
                              {2.20f, 1.30f, 0.30f}),
                MaterialId::PreparedStone, 0.72f});
        return result;
    }();
    return values;
}

bool PyramidInterior::blockIntersectsVoid(const PyramidBlockPlacement& block,
                                          std::size_t* regionIndex)
{
    for (std::size_t index = 0; index < passages().size(); ++index)
        if (intersectsPassage(block, passages()[index]))
        {
            if (regionIndex != nullptr) *regionIndex = index;
            return true;
        }
    for (std::size_t index = 0; index < rooms().size(); ++index)
        if (intersectsRoom(block, rooms()[index]))
        {
            if (regionIndex != nullptr) *regionIndex = passages().size() + index;
            return true;
        }
    return false;
}

bool PyramidInterior::isCutawayBlock(const PyramidBlockPlacement& block)
{
    return block.position.x > 0.25f && block.position.y >= 2.0f;
}

InteriorExclusionStats PyramidInterior::exclusionStats(
    const std::vector<PyramidBlockPlacement>& blocks)
{
    InteriorExclusionStats result;
    result.minimumLevel = std::numeric_limits<unsigned int>::max();
    result.minimumLevelByRegion.fill(std::numeric_limits<unsigned int>::max());
    for (const PyramidBlockPlacement& block : blocks)
    {
        std::size_t region = 0;
        if (!blockIntersectsVoid(block, &region))
            continue;
        ++result.total;
        ++result.byRegion[region];
        result.minimumLevelByRegion[region] =
            std::min(result.minimumLevelByRegion[region], block.level);
        result.maximumLevelByRegion[region] =
            std::max(result.maximumLevelByRegion[region], block.level);
        result.minimumLevel = std::min(result.minimumLevel, block.level);
        result.maximumLevel = std::max(result.maximumLevel, block.level);
    }
    if (result.total == 0)
        result.minimumLevel = 0;
    for (std::size_t index = 0; index < result.byRegion.size(); ++index)
        if (result.byRegion[index] == 0u)
            result.minimumLevelByRegion[index] = 0u;
    return result;
}

glm::vec3 PyramidInterior::cameraStartPosition()
{
    glm::vec3 constrained;
    const glm::vec3 candidate{0.0f, 0.0f, -80.0f};
    passageCameraPosition(passages()[0], candidate, constrained);
    return constrained;
}

bool PyramidInterior::isWalkableCameraPosition(const glm::vec3& position)
{
    glm::vec3 constrained;
    for (const PassageSegment& passage : passages())
        if (passageCameraPosition(passage, position, constrained) &&
            std::abs(position.y - constrained.y) <= 0.40f)
            return true;
    for (const InteriorRoom& room : rooms())
        if (roomCameraPosition(room, position, constrained) &&
            std::abs(position.y - constrained.y) <= 0.40f)
            return true;
    return false;
}

glm::vec3 PyramidInterior::constrainCamera(const glm::vec3& previous,
                                           const glm::vec3& candidate)
{
    glm::vec3 constrained;
    for (const PassageSegment& passage : passages())
        if (passageCameraPosition(passage, candidate, constrained))
            return constrained;
    for (const InteriorRoom& room : rooms())
        if (roomCameraPosition(room, candidate, constrained))
            return constrained;
    return previous;
}

InteriorBounds PyramidInterior::bounds()
{
    InteriorBounds result{glm::vec3{std::numeric_limits<float>::max()},
                          glm::vec3{std::numeric_limits<float>::lowest()}};
    for (const PassageSegment& passage : passages())
    {
        const glm::vec3 expansion{0.5f * passage.width,
                                  passage.height, 0.5f * passage.width};
        result.minimum = glm::min(result.minimum,
                                  glm::min(passage.start, passage.end) - expansion);
        result.maximum = glm::max(result.maximum,
                                  glm::max(passage.start, passage.end) + expansion);
    }
    for (const InteriorRoom& room : rooms())
    {
        result.minimum = glm::min(result.minimum, room.center - room.halfExtents);
        result.maximum = glm::max(result.maximum, room.center + room.halfExtents);
    }
    return result;
}

const char* PyramidInterior::typeName(InteriorSpaceType type)
{
    switch (type)
    {
    case InteriorSpaceType::Passage: return "Passage";
    case InteriorSpaceType::Gallery: return "Gallery";
    case InteriorSpaceType::Antechamber: return "Antechamber";
    case InteriorSpaceType::TombChamber: return "TombChamber";
    default: return "Unknown";
    }
}

bool validatePyramidInteriorGeometry(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> blocks =
        PyramidLayout::generateComplete(config);
    const InteriorExclusionStats stats = PyramidInterior::exclusionStats(blocks);
    std::vector<const PyramidBlockPlacement*> retainedBlocks;
    retainedBlocks.reserve(blocks.size() - stats.total);
    for (const PyramidBlockPlacement& block : blocks)
        if (!PyramidInterior::blockIntersectsVoid(block))
            retainedBlocks.push_back(&block);
    std::size_t shellTouchingBlocks = 0;
    bool shellTouchesOnlyEntry = true;
    for (const PyramidBlockPlacement& block : blocks)
    {
        std::size_t region = 0;
        if (!PyramidInterior::blockIntersectsVoid(block, &region))
            continue;
        const float topY = block.position.y + 0.5f * block.scale.y;
        const float shellHalf = pyramidHalfExtentAt(topY);
        const bool touchesShell =
            std::abs(block.position.x - config.origin.x) +
                    0.5f * block.scale.x >= shellHalf - 0.05f ||
            std::abs(block.position.z - config.origin.z) +
                    0.5f * block.scale.z >= shellHalf - 0.05f;
        if (touchesShell)
        {
            ++shellTouchingBlocks;
            shellTouchesOnlyEntry = shellTouchesOnlyEntry && region == 0u;
        }
    }
    bool voidClear = true;
    std::size_t samples = 0;
    for (const PassageSegment& passage : PyramidInterior::passages())
        for (int step = 0; step <= 20; ++step)
        {
            const float t = static_cast<float>(step) / 20.0f;
            const glm::vec3 floor = glm::mix(passage.start, passage.end, t);
            for (float x : {-0.35f * passage.width, 0.0f,
                            0.35f * passage.width})
                for (float y : {0.35f, 0.5f * passage.height,
                                passage.height - 0.35f})
                {
                    const glm::vec3 point = floor + glm::vec3{x, y, 0.0f};
                    ++samples;
                    for (const PyramidBlockPlacement* block : retainedBlocks)
                        if (blockContainsPoint(*block, point))
                            voidClear = false;
                }
        }
    for (const InteriorRoom& room : PyramidInterior::rooms())
        for (const glm::vec3& offset : {glm::vec3{0.0f},
                                        glm::vec3{room.halfExtents.x * 0.65f, 0.0f, 0.0f},
                                        glm::vec3{-room.halfExtents.x * 0.65f, 0.0f, 0.0f},
                                        glm::vec3{0.0f, 0.0f, room.halfExtents.z * 0.65f}})
        {
            const glm::vec3 point = room.center + offset;
            ++samples;
            for (const PyramidBlockPlacement* block : retainedBlocks)
                if (blockContainsPoint(*block, point))
                    voidClear = false;
        }

    bool insidePyramid = true;
    for (std::size_t passageIndex = 0;
         passageIndex < PyramidInterior::passages().size(); ++passageIndex)
    {
        const PassageSegment& passage = PyramidInterior::passages()[passageIndex];
        const int firstSample = passageIndex == 0 ? 8 : 0;
        for (int step = firstSample; step <= 20; ++step)
        {
            const glm::vec3 point = glm::mix(
                passage.start, passage.end, static_cast<float>(step) / 20.0f);
            const float half = pyramidHalfExtentAt(point.y);
            insidePyramid = insidePyramid &&
                std::abs(point.x - config.origin.x) +
                    0.5f * passage.width < half;
            const float relativeZ = std::abs(point.z - config.origin.z);
            insidePyramid = insidePyramid && relativeZ + 0.5f * passage.width < half;
        }
    }
    for (const InteriorRoom& room : PyramidInterior::rooms())
    {
        const float half = pyramidHalfExtentAt(room.center.y + room.halfExtents.y);
        insidePyramid = insidePyramid &&
            std::abs(room.center.x) + room.halfExtents.x < half &&
            std::abs(room.center.z - config.origin.z) + room.halfExtents.z < half;
    }

    const bool countValid = blocks.size() == 7714u && stats.total > 0u &&
                            stats.total < 400u;
    bool timelineFilteringValid = true;
    const std::array<float, 5> checkpoints{{0.0f, 0.25f, 0.50f, 0.75f, 1.0f}};
    std::array<std::size_t, 5> renderedAtCheckpoint{};
    for (std::size_t checkpointIndex = 0;
         checkpointIndex < checkpoints.size(); ++checkpointIndex)
    {
        ConstructionTimelineController timeline;
        timeline.setProgress(checkpoints[checkpointIndex]);
        const std::size_t scheduled = timeline.visibleBlockCount(blocks, config);
        std::size_t excludedScheduled = 0;
        std::size_t rendered = 0;
        for (const PyramidBlockPlacement& block : blocks)
        {
            const ConstructionBlockState state = timeline.blockState(block, config);
            if (!state.visible)
                continue;
            if (PyramidInterior::blockIntersectsVoid(block))
                ++excludedScheduled;
            else
                ++rendered;
        }
        renderedAtCheckpoint[checkpointIndex] = rendered;
        timelineFilteringValid = timelineFilteringValid &&
                                 rendered + excludedScheduled == scheduled;
    }
    const bool oneExteriorOpening = shellTouchingBlocks > 0u &&
                                    shellTouchesOnlyEntry;
    const bool sarcophagusSupported = std::abs(16.0f -
        (PyramidInterior::rooms()[1].center.y -
         PyramidInterior::rooms()[1].halfExtents.y)) < 1.0e-5f;
    const bool valid = countValid && voidClear && insidePyramid &&
                       timelineFilteringValid &&
                       oneExteriorOpening && sarcophagusSupported;
    output << "Phase 12.8 pyramid interior geometry validation\n"
           << "  generated blocks: " << blocks.size() << '\n'
           << "  excluded interior blocks: " << stats.total << '\n'
           << "  rendered structural blocks: " << blocks.size() - stats.total << '\n'
           << "  exclusions by region:";
    for (std::size_t index = 0; index < stats.byRegion.size(); ++index)
        output << ' ' << stats.byRegion[index] << "(L"
               << stats.minimumLevelByRegion[index] << '-'
               << stats.maximumLevelByRegion[index] << ')';
    output << "\n  shell-touching excluded blocks: " << shellTouchingBlocks
           << (oneExteriorOpening ? " (entry region only) PASS\n" : " FAIL\n")
           << "  walkable void samples: " << samples << (voidClear ? " PASS\n" : " FAIL\n")
           << "  interior remains inside shell except entrance: "
           << (insidePyramid ? "PASS" : "FAIL") << '\n'
           << "  intentional exterior openings: 1 "
           << (oneExteriorOpening ? "PASS\n" : "FAIL\n")
           << "  sarcophagus floor support: "
           << (sarcophagusSupported ? "PASS" : "FAIL") << '\n'
           << "  rendered after exclusion at 0/25/50/75/100%:";
    for (std::size_t count : renderedAtCheckpoint)
        output << ' ' << count;
    output << (timelineFilteringValid ? " PASS\n" : " FAIL\n")
           << (valid ? "Interior geometry checks passed.\n"
                     : "Interior geometry checks failed.\n");
    return valid;
}

bool validatePyramidInteriorConnectivity(std::ostream& output)
{
    bool passagesConnect = true;
    const auto& passages = PyramidInterior::passages();
    for (std::size_t index = 1; index < passages.size(); ++index)
        passagesConnect = passagesConnect &&
                          glm::length(passages[index - 1].end -
                                      passages[index].start) < 0.01f;
    const InteriorRoom& antechamber = PyramidInterior::rooms()[0];
    const InteriorRoom& tomb = PyramidInterior::rooms()[1];
    const bool passageToAntechamber =
        std::abs(passages.back().end.z - antechamber.center.z) <=
        antechamber.halfExtents.z;
    const bool roomsOverlap =
        std::abs(antechamber.center.z - tomb.center.z) <=
        antechamber.halfExtents.z + tomb.halfExtents.z;
    const bool valid = passagesConnect && passageToAntechamber && roomsOverlap;
    output << "Phase 12.8 interior connectivity validation\n"
           << "  passage chain: " << (passagesConnect ? "PASS" : "FAIL") << '\n'
           << "  gallery to antechamber: "
           << (passageToAntechamber ? "PASS" : "FAIL") << '\n'
           << "  antechamber to tomb: " << (roomsOverlap ? "PASS" : "FAIL") << '\n'
           << (valid ? "Interior connectivity checks passed.\n"
                     : "Interior connectivity checks failed.\n");
    return valid;
}

bool validatePyramidInteriorNavigation(std::ostream& output)
{
    bool valid = true;
    std::size_t positions = 0;
    for (const PassageSegment& passage : PyramidInterior::passages())
        for (float t : {0.10f, 0.50f, 0.90f})
        {
            const glm::vec3 floor = glm::mix(passage.start, passage.end, t);
            const glm::vec3 position = floor + glm::vec3{0.0f,
                                                         PyramidInterior::eyeHeight,
                                                         0.0f};
            valid = valid && finite(position) &&
                    PyramidInterior::isWalkableCameraPosition(position) &&
                    PyramidInterior::eyeHeight < passage.height - 0.25f;
            ++positions;
        }
    for (const InteriorRoom& room : PyramidInterior::rooms())
    {
        const glm::vec3 position{room.center.x,
                                 room.center.y - room.halfExtents.y +
                                     PyramidInterior::eyeHeight,
                                 room.center.z};
        valid = valid && PyramidInterior::isWalkableCameraPosition(position) &&
                position.y < room.center.y + room.halfExtents.y;
        ++positions;
    }
    const glm::vec3 start = PyramidInterior::cameraStartPosition();
    const glm::vec3 rejected = PyramidInterior::constrainCamera(
        start, start + glm::vec3{20.0f, 0.0f, 0.0f});
    const bool confinement = glm::length(rejected - start) < 1.0e-5f;
    glm::vec3 walked = start;
    bool traversalValid = true;
    for (int step = 0; step < 560; ++step)
    {
        const glm::vec3 next = PyramidInterior::constrainCamera(
            walked, walked + glm::vec3{0.0f, 0.0f, 0.10f});
        if (!finite(next) || glm::length(next - walked) < 0.001f)
        {
            traversalValid = false;
            break;
        }
        walked = next;
        if (walked.z >= PyramidInterior::rooms()[1].center.z)
            break;
    }
    traversalValid = traversalValid &&
                     walked.z >= PyramidInterior::rooms()[1].center.z - 0.10f &&
                     PyramidInterior::isWalkableCameraPosition(walked);
    const bool lightValid = std::isfinite(PyramidInterior::inspectionLightRange) &&
                            PyramidInterior::inspectionLightRange >= 8.0f &&
                            PyramidInterior::inspectionLightRange <= 15.0f &&
                            PyramidInterior::inspectionLightIntensity > 0.0f;
    valid = valid && confinement && traversalValid && lightValid;
    output << "Phase 12.8 interior navigation validation\n"
           << "  representative positions: " << positions
           << (valid ? " PASS\n" : " FAIL\n")
           << "  wall confinement: " << (confinement ? "PASS" : "FAIL") << '\n'
           << "  entrance-to-tomb step traversal: "
           << (traversalValid ? "PASS" : "FAIL") << '\n'
           << "  eye height: " << PyramidInterior::eyeHeight << "\n"
           << "  inspection light range: " << PyramidInterior::inspectionLightRange
           << (lightValid ? " PASS\n" : " FAIL\n")
           << (valid ? "Interior navigation checks passed.\n"
                     : "Interior navigation checks failed.\n");
    return valid;
}

bool validatePyramidInterior(std::ostream& output)
{
    return validatePyramidInteriorGeometry(output) &&
           validatePyramidInteriorConnectivity(output) &&
           validatePyramidInteriorNavigation(output);
}
