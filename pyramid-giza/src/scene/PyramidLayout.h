#pragma once

#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

struct PyramidLayoutConfig
{
    PyramidLayoutConfig();

    unsigned int baseBlocksPerSide;
    unsigned int completedLevels;
    unsigned int partialFromLevel;
    float blockWidth;
    float blockHeight;
    float blockDepth;
    float horizontalSpacing;
    glm::vec3 origin;
};

struct PyramidBlockPlacement
{
    glm::vec3 position;
    glm::vec3 scale;
    unsigned int level = 0;
    unsigned int gridX = 0;
    unsigned int gridZ = 0;
};

struct PyramidLayoutStats
{
    std::size_t totalBlocks = 0;
    std::vector<std::size_t> blocksPerLevel;
    float baseWidth = 0.0f;
    float baseDepth = 0.0f;
    float completedHeight = 0.0f;
};

class PyramidLayout
{
public:
    static std::vector<PyramidBlockPlacement> generate(const PyramidLayoutConfig& config);
    static std::vector<PyramidBlockPlacement> generateComplete(const PyramidLayoutConfig& config);
    static bool isLegacyConstructionOpening(const PyramidLayoutConfig& config,
                                            const PyramidBlockPlacement& block);
    static PyramidLayoutStats statistics(const PyramidLayoutConfig& config,
                                          const std::vector<PyramidBlockPlacement>& blocks);
};

bool validatePyramidLayout(std::ostream& output);
