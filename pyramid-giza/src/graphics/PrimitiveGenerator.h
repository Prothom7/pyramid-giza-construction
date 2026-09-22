#pragma once

#include "graphics/Mesh.h"

class PrimitiveGenerator
{
public:
    static MeshData createTriangle();
    static MeshData createPlane();
    static MeshData createCube();
    static MeshData createPyramid();
    static MeshData createDisk(float radius = 0.5f, unsigned int segments = 32);
    static MeshData createCylinder(float radius = 0.5f, float height = 1.0f,
                                   unsigned int segments = 32);
    static MeshData createSphere(float radius = 0.5f, unsigned int latitudeSegments = 32,
                                 unsigned int longitudeSegments = 32);
};
