#pragma once

#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

struct PyramidLayoutConfig
{
    unsigned int baseBlocksPerSide = 15;
    unsigned int completedLevels = 10;
    unsigned int partialFromLevel = 6;
    float blockWidth = 1.15f;
    float blockHeight = 0.62f;
    float blockDepth = 1.15f;
    float horizontalSpacing = 0.07f;
    glm::vec3 origin{0.0f, 0.0f, -10.0f};
};

struct PyramidBlockPlacement
{
    glm::vec3 position;
    glm::vec3 scale;
    unsigned int level = 0;
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
    static PyramidLayoutStats statistics(const PyramidLayoutConfig& config,
                                          const std::vector<PyramidBlockPlacement>& blocks);
};

bool validatePyramidLayout(std::ostream& output);
