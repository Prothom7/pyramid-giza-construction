#include "scene/PyramidLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>

#include "scene/MonumentalSite.h"

namespace
{
void validateConfig(const PyramidLayoutConfig& config)
{
    if (config.baseBlocksPerSide == 0 || config.completedLevels == 0)
        throw std::invalid_argument("Pyramid block and level counts must be positive");
    if (config.completedLevels > config.baseBlocksPerSide)
        throw std::invalid_argument("Completed levels cannot exceed the base block count");
    if (config.partialFromLevel > config.completedLevels)
        throw std::invalid_argument("Partial-level start is outside the completed range");
    if (config.blockWidth <= 0.0f || config.blockHeight <= 0.0f ||
        config.blockDepth <= 0.0f || config.horizontalSpacing < 0.0f)
        throw std::invalid_argument("Pyramid dimensions and spacing are invalid");
}

bool isConstructionOpening(const PyramidLayoutConfig& config, unsigned int level,
                           unsigned int side, unsigned int x, unsigned int z)
{
    if (level < config.partialFromLevel)
        return false;

    const unsigned int progress = level - config.partialFromLevel;
    const unsigned int missingRows = 1 + progress / 2;
    const float openingHalfWidth = static_cast<float>(1 + progress / 2);
    const float center = 0.5f * static_cast<float>(side - 1);
    const bool inFrontRows = z + missingRows >= side;
    const bool inOpeningWidth = std::abs(static_cast<float>(x) - center) <= openingHalfWidth;
    const bool preservesSideEdges = x > 0 && x + 1 < side;
    return inFrontRows && inOpeningWidth && preservesSideEdges;
}

bool finiteVector(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace

PyramidLayoutConfig::PyramidLayoutConfig()
{
    const WorldScale& world = MonumentalSite::scale();
    baseBlocksPerSide = world.pyramidBaseBlocks;
    completedLevels = world.pyramidCompletedLevels;
    partialFromLevel = world.pyramidPartialFromLevel;
    blockWidth = world.blockWidth;
    blockHeight = world.blockHeight;
    blockDepth = world.blockDepth;
    horizontalSpacing = world.blockSpacing;
    origin = world.pyramidOrigin;
}

std::vector<PyramidBlockPlacement> PyramidLayout::generate(const PyramidLayoutConfig& config)
{
    validateConfig(config);
    std::vector<PyramidBlockPlacement> blocks;
    const float xStep = config.blockWidth + config.horizontalSpacing;
    const float zStep = config.blockDepth + config.horizontalSpacing;

    for (unsigned int level = 0; level < config.completedLevels; ++level)
    {
        const unsigned int side = config.baseBlocksPerSide - level;
        const float centerOffset = 0.5f * static_cast<float>(side - 1);
        const float y = config.origin.y + (static_cast<float>(level) + 0.5f) * config.blockHeight;

        for (unsigned int z = 0; z < side; ++z)
        {
            for (unsigned int x = 0; x < side; ++x)
            {
                if (isConstructionOpening(config, level, side, x, z))
                    continue;

                blocks.push_back({
                    {config.origin.x + (static_cast<float>(x) - centerOffset) * xStep,
                     y,
                     config.origin.z + (static_cast<float>(z) - centerOffset) * zStep},
                    {config.blockWidth, config.blockHeight, config.blockDepth},
                    level});
            }
        }
    }
    return blocks;
}

PyramidLayoutStats PyramidLayout::statistics(
    const PyramidLayoutConfig& config, const std::vector<PyramidBlockPlacement>& blocks)
{
    PyramidLayoutStats stats;
    stats.totalBlocks = blocks.size();
    stats.blocksPerLevel.assign(config.completedLevels, 0);
    for (const PyramidBlockPlacement& block : blocks)
    {
        if (block.level < stats.blocksPerLevel.size())
            ++stats.blocksPerLevel[block.level];
    }
    stats.baseWidth = config.baseBlocksPerSide * config.blockWidth +
                      (config.baseBlocksPerSide - 1) * config.horizontalSpacing;
    stats.baseDepth = config.baseBlocksPerSide * config.blockDepth +
                      (config.baseBlocksPerSide - 1) * config.horizontalSpacing;
    stats.completedHeight = config.completedLevels * config.blockHeight;
    return stats;
}

bool validatePyramidLayout(std::ostream& output)
{
    const PyramidLayoutConfig config;
    const std::vector<PyramidBlockPlacement> first = PyramidLayout::generate(config);
    const std::vector<PyramidBlockPlacement> second = PyramidLayout::generate(config);
    const PyramidLayoutStats stats = PyramidLayout::statistics(config, first);
    bool valid = !first.empty() && first.size() == second.size();

    for (std::size_t index = 0; index < first.size(); ++index)
    {
        const PyramidBlockPlacement& a = first[index];
        const PyramidBlockPlacement& b = second[index];
        valid = valid && finiteVector(a.position) && finiteVector(a.scale);
        valid = valid && glm::all(glm::lessThan(glm::abs(a.position - b.position), glm::vec3(1.0e-6f)));
        valid = valid && glm::all(glm::greaterThan(a.scale, glm::vec3(0.0f)));
        const float expectedY = config.origin.y +
                                (static_cast<float>(a.level) + 0.5f) * config.blockHeight;
        valid = valid && std::abs(a.position.y - expectedY) < 1.0e-5f;
    }

    for (unsigned int level = 0; level < config.completedLevels; ++level)
    {
        valid = valid && stats.blocksPerLevel[level] > 0;
        if (level > 0)
            valid = valid && stats.blocksPerLevel[level] < stats.blocksPerLevel[level - 1];

        float minimumX = std::numeric_limits<float>::max();
        float maximumX = std::numeric_limits<float>::lowest();
        float minimumZ = std::numeric_limits<float>::max();
        float maximumZ = std::numeric_limits<float>::lowest();
        for (const PyramidBlockPlacement& block : first)
        {
            if (block.level != level)
                continue;
            minimumX = std::min(minimumX, block.position.x);
            maximumX = std::max(maximumX, block.position.x);
            minimumZ = std::min(minimumZ, block.position.z);
            maximumZ = std::max(maximumZ, block.position.z);
        }
        valid = valid && std::abs(0.5f * (minimumX + maximumX) - config.origin.x) < 1.0e-5f;
        valid = valid && std::abs(0.5f * (minimumZ + maximumZ) - config.origin.z) < 1.0e-5f;
    }

    valid = valid && stats.totalBlocks == 7561;
    output << "Phase 2 pyramid layout validation\n"
           << "  completed levels: " << config.completedLevels << '\n'
           << "  blocks per level:";
    for (std::size_t count : stats.blocksPerLevel)
        output << ' ' << count;
    output << "\n  total blocks: " << stats.totalBlocks << '\n'
           << "  base footprint: " << stats.baseWidth << " x " << stats.baseDepth << '\n'
           << "  completed height: " << stats.completedHeight << '\n'
           << (valid ? "Pyramid layout checks passed.\n" : "Pyramid layout checks failed.\n");
    return valid;
}
