#pragma once

#include <cstddef>
#include <iosfwd>
#include <vector>

#include <glm/glm.hpp>

#include "scene/SceneTypes.h"

struct MeshData;

struct QuarryConfig
{
    glm::vec3 center{-128.0f, 0.0f, -15.0f};
    float width = 64.0f;
    float depth = 72.0f;
    float excavationDepth = 7.5f;
    float floorWidth = 42.0f;
    float floorDepth = 34.0f;
};

struct QuarryTerraceDescriptor
{
    const char* id;
    float elevation;
    float inset;
    const char* purpose;
};

struct ExtractionBayDescriptor
{
    const char* id;
    glm::vec3 center;
    const char* stage;
};

struct RepositoryDescriptor
{
    const char* id;
    const char* type;
    glm::vec3 center;
    unsigned int rows;
    unsigned int columns;
    unsigned int levels;
    glm::vec3 blockScale;
    float spacing;
    MaterialId material;
};

struct LogisticsStageDescriptor
{
    const char* id;
    unsigned int order;
    glm::vec3 center;
};

struct HeavyLiftingRigDescriptor
{
    glm::vec3 center;
    float width;
    float height;
    float depth;
};

struct EnvironmentalContext
{
    glm::vec3 nileCenter{0.0f, 0.03f, -166.0f};
    glm::vec2 nileSize{330.0f, 26.0f};
    float nileEndTaperLength = 25.0f;
    float nileTerminalWidth = 0.6f;
    glm::vec2 floodplainSize{340.0f, 20.0f};
    float floodplainY = 0.02f;
    glm::vec3 sphinxCenter{92.0f, 0.0f, -105.0f};
};

// Derived from EnvironmentalContext; there is no second authored water level
// or footprint. The rendered strip and bounded queries use these same banks.
struct NileSurfaceBounds
{
    float minX;
    float maxX;
    float minZ;
    float maxZ;
    float waterY;
    float endTaperLength;
    float terminalWidth;

    bool contains(float x, float z) const;
    float northBankAt(float x) const;
    float southBankAt(float x) const;
    float signedBankDistance(float x, float z) const;
};

class IndustrialLandscape
{
public:
    static constexpr int NileLengthSegments = 66; // 5 m stations preserve every taper control point.
    static constexpr int NileWidthSegments = 13;  // 2 m across the full-width channel.
    static const QuarryConfig& quarry();
    static const std::vector<QuarryTerraceDescriptor>& terraces();
    static const std::vector<ExtractionBayDescriptor>& extractionBays();
    static const std::vector<RepositoryDescriptor>& repositories();
    static const std::vector<LogisticsStageDescriptor>& logisticsStages();
    static const HeavyLiftingRigDescriptor& liftingRig();
    static const EnvironmentalContext& environment();
    static NileSurfaceBounds nileSurface();
    static glm::mat4 nileSurfaceModel();
    static MeshData nileSurfaceMesh();
    static glm::mat4 floodplainModel();
    static bool isInsideNile(float x, float z);
    static bool waterHeightAt(float x, float z, float& height);
    static const std::vector<glm::vec3>& treePositions();
    static std::size_t repositoryBlockCount(const RepositoryDescriptor& repository);
};

bool validateIndustrialLandscape(std::ostream& output);
