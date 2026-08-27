#pragma once

#include <vector>
#include <glm/glm.hpp>

// Represents one stone block's position in the world. We keep this
// lightweight -- just a position for now -- since rotation/scale are
// identical for every block (we're not doing weathering/variation yet).
struct BlockInstance
{
    glm::vec3 position;
};

class Pyramid
{
public:
    // baseSize: width of the bottom course, in world units.
    // numCourses: how many horizontal layers stack up to the top.
    // blockSize: size of one cubic stone block, in world units.
    Pyramid(float baseSize, int numCourses, float blockSize);

    // Generates all block positions based on the constructor parameters.
    // Called once, from the constructor -- not every frame.
    void generate();

    // Returns the list of block positions, so main.cpp can loop over
    // them and issue a draw call (with the appropriate Model matrix)
    // for each one.
    const std::vector<BlockInstance>& getBlocks() const;

    // How many blocks currently exist in total (useful for debug/UI
    // output and later, performance comparisons before/after instancing).
    size_t getBlockCount() const;

private:
    float m_baseSize;
    int m_numCourses;
    float m_blockSize;

    std::vector<BlockInstance> m_blocks;
};