#include "Pyramid.h"

Pyramid::Pyramid(float baseSize, int numCourses, float blockSize)
    : m_baseSize(baseSize), m_numCourses(numCourses), m_blockSize(blockSize)
{
    generate();
}

void Pyramid::generate()
{
    m_blocks.clear();

    // Loop over each horizontal course, from the bottom (i=0) to the top.
    for (int i = 0; i < m_numCourses; i++)
    {
        // Linearly shrink this course's width as we go up, per our
        // Step 1 formula.
        float courseWidth = m_baseSize * (1.0f - (float)i / (float)m_numCourses);

        // How many blocks fit across this course, at least 1 so the
        // very top course isn't empty.
        int blocksPerSide = std::max(1, (int)(courseWidth / m_blockSize));

        // This course's height above the ground.
        float courseHeight = i * m_blockSize;

        // Loop over every block position within this course's grid.
        for (int x = 0; x < blocksPerSide; x++)
        {
            for (int z = 0; z < blocksPerSide; z++)
            {
                // Center the grid on the world origin by offsetting by
                // half the total grid width.
                float worldX = (x - blocksPerSide / 2.0f) * m_blockSize;
                float worldZ = (z - blocksPerSide / 2.0f) * m_blockSize;

                BlockInstance block;
                block.position = glm::vec3(worldX, courseHeight, worldZ);
                m_blocks.push_back(block);
            }
        }
    }
}

const std::vector<BlockInstance>& Pyramid::getBlocks() const
{
    return m_blocks;
}

size_t Pyramid::getBlockCount() const
{
    return m_blocks.size();
}