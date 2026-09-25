#include "graphics/GeometryValidation.h"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <ostream>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "graphics/PrimitiveGenerator.h"

GeometryValidationResult validateMeshData(const MeshData& mesh)
{
    GeometryValidationResult result;
    const auto fail = [&](const std::string& message) {
        result.valid = false;
        result.errors.push_back(message);
    };

    if (mesh.vertices.empty())
        fail("has no vertices");
    if (mesh.indices.empty())
        fail("has no indices");
    if (mesh.indices.size() % 3 != 0)
        fail("index count is not divisible by three");

    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        const Vertex& vertex = mesh.vertices[i];
        const float normalLength = glm::length(vertex.normal);
        if (!std::isfinite(normalLength) || std::abs(normalLength - 1.0f) > 0.001f)
            fail("vertex " + std::to_string(i) + " has a non-unit normal");
        const glm::vec3 absolutePosition = glm::abs(vertex.position);
        if (absolutePosition.x > 0.5001f || absolutePosition.y > 0.5001f ||
            absolutePosition.z > 0.5001f)
            fail("vertex " + std::to_string(i) + " leaves canonical [-0.5,+0.5] bounds");
    }

    for (std::size_t triangle = 0; triangle + 2 < mesh.indices.size(); triangle += 3)
    {
        const std::uint32_t ia = mesh.indices[triangle];
        const std::uint32_t ib = mesh.indices[triangle + 1];
        const std::uint32_t ic = mesh.indices[triangle + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size())
        {
            fail("triangle " + std::to_string(triangle / 3) + " has an out-of-range index");
            continue;
        }

        const Vertex& a = mesh.vertices[ia];
        const Vertex& b = mesh.vertices[ib];
        const Vertex& c = mesh.vertices[ic];
        const glm::vec3 geometricNormal = glm::cross(b.position - a.position, c.position - a.position);
        if (glm::dot(geometricNormal, geometricNormal) < 1.0e-12f)
        {
            fail("triangle " + std::to_string(triangle / 3) + " is degenerate");
            continue;
        }

        if (glm::dot(geometricNormal, a.normal) <= 0.0f ||
            glm::dot(geometricNormal, b.normal) <= 0.0f ||
            glm::dot(geometricNormal, c.normal) <= 0.0f)
            fail("triangle " + std::to_string(triangle / 3) +
                 " winding disagrees with an outward vertex normal");
    }
    return result;
}

bool validatePrimitiveFoundation(std::ostream& output)
{
    struct ExpectedMesh
    {
        MeshData mesh;
        std::size_t vertices;
        std::size_t indices;
    };

    std::vector<ExpectedMesh> meshes;
    meshes.push_back({PrimitiveGenerator::createTriangle(), 3, 3});
    meshes.push_back({PrimitiveGenerator::createPlane(), 4, 6});
    meshes.push_back({PrimitiveGenerator::createCube(), 24, 36});
    meshes.push_back({PrimitiveGenerator::createPyramid(), 16, 18});
    meshes.push_back({PrimitiveGenerator::createDisk(), 33, 96});
    meshes.push_back({PrimitiveGenerator::createCylinder(), 132, 384});
    meshes.push_back({PrimitiveGenerator::createSphere(), 1025, 5952});

    bool allValid = true;
    output << "Phase 1 CPU geometry validation\n";
    output << std::left << std::setw(12) << "Primitive" << std::right << std::setw(10)
           << "Vertices" << std::setw(10) << "Indices" << std::setw(11) << "Triangles" << '\n';

    for (const ExpectedMesh& expected : meshes)
    {
        GeometryValidationResult result = validateMeshData(expected.mesh);
        if (expected.mesh.vertices.size() != expected.vertices)
        {
            result.valid = false;
            result.errors.push_back("unexpected vertex count");
        }
        if (expected.mesh.indices.size() != expected.indices)
        {
            result.valid = false;
            result.errors.push_back("unexpected index count");
        }

        output << std::left << std::setw(12) << expected.mesh.name << std::right << std::setw(10)
               << expected.mesh.vertices.size() << std::setw(10) << expected.mesh.indices.size()
               << std::setw(11) << expected.mesh.indices.size() / 3
               << (result.valid ? "  PASS" : "  FAIL") << '\n';
        for (const std::string& error : result.errors)
            output << "  - " << error << '\n';
        allValid = allValid && result.valid;
    }

    output << (allValid ? "All primitive geometry checks passed.\n"
                        : "Primitive geometry validation failed.\n");
    return allValid;
}
