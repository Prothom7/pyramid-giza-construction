#include "graphics/PrimitiveGenerator.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

#include <glm/gtc/constants.hpp>

namespace
{
Vertex makeVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv)
{
    return {position, normal, uv};
}

void requirePositive(float value, const char* parameter)
{
    if (value <= 0.0f)
        throw std::invalid_argument(std::string(parameter) + " must be positive");
}

void requireSegments(unsigned int segments, unsigned int minimum, const char* parameter)
{
    if (segments < minimum)
        throw std::invalid_argument(std::string(parameter) + " is below the valid minimum");
}

void appendFace(MeshData& mesh, const std::array<glm::vec3, 4>& positions,
                const glm::vec3& normal)
{
    const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(makeVertex(positions[0], normal, {0.0f, 0.0f}));
    mesh.vertices.push_back(makeVertex(positions[1], normal, {1.0f, 0.0f}));
    mesh.vertices.push_back(makeVertex(positions[2], normal, {1.0f, 1.0f}));
    mesh.vertices.push_back(makeVertex(positions[3], normal, {0.0f, 1.0f}));
    mesh.indices.insert(mesh.indices.end(),
                        {first, first + 1, first + 2, first, first + 2, first + 3});
}

void appendFlatTriangle(MeshData& mesh, const glm::vec3& a, const glm::vec3& b,
                        const glm::vec3& c)
{
    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(makeVertex(a, normal, {0.0f, 0.0f}));
    mesh.vertices.push_back(makeVertex(b, normal, {1.0f, 0.0f}));
    mesh.vertices.push_back(makeVertex(c, normal, {0.5f, 1.0f}));
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2});
}
} // namespace

MeshData PrimitiveGenerator::createTriangle()
{
    MeshData mesh{"Triangle"};
    const glm::vec3 normal{0.0f, 0.0f, 1.0f};
    mesh.vertices = {
        makeVertex({-0.5f, -0.5f, 0.0f}, normal, {0.0f, 0.0f}),
        makeVertex({0.5f, -0.5f, 0.0f}, normal, {1.0f, 0.0f}),
        makeVertex({0.0f, 0.5f, 0.0f}, normal, {0.5f, 1.0f})};
    mesh.indices = {0, 1, 2};
    return mesh;
}

MeshData PrimitiveGenerator::createPlane()
{
    MeshData mesh{"Plane"};
    const glm::vec3 normal{0.0f, 1.0f, 0.0f};
    mesh.vertices = {
        makeVertex({-0.5f, 0.0f, -0.5f}, normal, {0.0f, 0.0f}),
        makeVertex({0.5f, 0.0f, -0.5f}, normal, {1.0f, 0.0f}),
        makeVertex({0.5f, 0.0f, 0.5f}, normal, {1.0f, 1.0f}),
        makeVertex({-0.5f, 0.0f, 0.5f}, normal, {0.0f, 1.0f})};
    // From +Y the XZ plane is CCW in this order.
    mesh.indices = {0, 2, 1, 0, 3, 2};
    return mesh;
}

MeshData PrimitiveGenerator::createCube()
{
    MeshData mesh{"Cube"};
    constexpr float n = -0.5f;
    constexpr float p = 0.5f;

    appendFace(mesh, {{{n, n, p}, {p, n, p}, {p, p, p}, {n, p, p}}}, {0, 0, 1});
    appendFace(mesh, {{{p, n, n}, {n, n, n}, {n, p, n}, {p, p, n}}}, {0, 0, -1});
    appendFace(mesh, {{{p, n, p}, {p, n, n}, {p, p, n}, {p, p, p}}}, {1, 0, 0});
    appendFace(mesh, {{{n, n, n}, {n, n, p}, {n, p, p}, {n, p, n}}}, {-1, 0, 0});
    appendFace(mesh, {{{n, p, p}, {p, p, p}, {p, p, n}, {n, p, n}}}, {0, 1, 0});
    appendFace(mesh, {{{n, n, n}, {p, n, n}, {p, n, p}, {n, n, p}}}, {0, -1, 0});
    return mesh;
}

MeshData PrimitiveGenerator::createPyramid()
{
    MeshData mesh{"Pyramid"};
    const glm::vec3 apex{0.0f, 0.5f, 0.0f};
    const glm::vec3 p0{-0.5f, -0.5f, -0.5f};
    const glm::vec3 p1{0.5f, -0.5f, -0.5f};
    const glm::vec3 p2{0.5f, -0.5f, 0.5f};
    const glm::vec3 p3{-0.5f, -0.5f, 0.5f};

    appendFlatTriangle(mesh, p3, p2, apex);
    appendFlatTriangle(mesh, p2, p1, apex);
    appendFlatTriangle(mesh, p1, p0, apex);
    appendFlatTriangle(mesh, p0, p3, apex);
    appendFace(mesh, {{p0, p1, p2, p3}}, {0.0f, -1.0f, 0.0f});
    return mesh;
}

MeshData PrimitiveGenerator::createDisk(float radius, unsigned int segments)
{
    requirePositive(radius, "radius");
    requireSegments(segments, 3, "segments");

    MeshData mesh{"Disk"};
    const glm::vec3 normal{0.0f, 1.0f, 0.0f};
    mesh.vertices.reserve(segments + 1);
    mesh.indices.reserve(segments * 3);
    mesh.vertices.push_back(makeVertex({0, 0, 0}, normal, {0.5f, 0.5f}));

    for (unsigned int i = 0; i < segments; ++i)
    {
        const float theta = glm::two_pi<float>() * static_cast<float>(i) /
                            static_cast<float>(segments);
        const float x = radius * std::cos(theta);
        const float z = radius * std::sin(theta);
        mesh.vertices.push_back(makeVertex({x, 0.0f, z}, normal,
                                           {0.5f + x / (2.0f * radius),
                                            0.5f + z / (2.0f * radius)}));
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        const std::uint32_t current = 1 + i;
        const std::uint32_t next = 1 + ((i + 1) % segments);
        // theta grows toward +Z. Reversing perimeter order makes +Y the front.
        mesh.indices.insert(mesh.indices.end(), {0, next, current});
    }
    return mesh;
}

MeshData PrimitiveGenerator::createCylinder(float radius, float height, unsigned int segments)
{
    requirePositive(radius, "radius");
    requirePositive(height, "height");
    requireSegments(segments, 3, "segments");

    MeshData mesh{"Cylinder"};
    const float halfHeight = height * 0.5f;
    mesh.vertices.reserve(4 * segments + 4);
    mesh.indices.reserve(segments * 12);

    // The duplicated seam sample has u=1 while the first has u=0.
    for (unsigned int i = 0; i <= segments; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float theta = glm::two_pi<float>() * u;
        const float cosine = std::cos(theta);
        const float sine = std::sin(theta);
        const glm::vec3 normal{cosine, 0.0f, sine};
        mesh.vertices.push_back(makeVertex({radius * cosine, -halfHeight, radius * sine},
                                           normal, {u, 0.0f}));
        mesh.vertices.push_back(makeVertex({radius * cosine, halfHeight, radius * sine},
                                           normal, {u, 1.0f}));
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        const std::uint32_t bottom = i * 2;
        const std::uint32_t top = bottom + 1;
        const std::uint32_t nextBottom = bottom + 2;
        const std::uint32_t nextTop = bottom + 3;
        mesh.indices.insert(mesh.indices.end(),
                            {bottom, top, nextBottom, top, nextTop, nextBottom});
    }

    const auto appendCap = [&](float y, const glm::vec3& normal, bool top) {
        const std::uint32_t center = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(makeVertex({0.0f, y, 0.0f}, normal, {0.5f, 0.5f}));
        const std::uint32_t rimStart = static_cast<std::uint32_t>(mesh.vertices.size());
        for (unsigned int i = 0; i < segments; ++i)
        {
            const float theta = glm::two_pi<float>() * static_cast<float>(i) /
                                static_cast<float>(segments);
            const float x = radius * std::cos(theta);
            const float z = radius * std::sin(theta);
            mesh.vertices.push_back(makeVertex({x, y, z}, normal,
                                               {0.5f + x / (2.0f * radius),
                                                0.5f + z / (2.0f * radius)}));
        }
        for (unsigned int i = 0; i < segments; ++i)
        {
            const std::uint32_t current = rimStart + i;
            const std::uint32_t next = rimStart + ((i + 1) % segments);
            if (top)
                mesh.indices.insert(mesh.indices.end(), {center, next, current});
            else
                mesh.indices.insert(mesh.indices.end(), {center, current, next});
        }
    };

    appendCap(halfHeight, {0.0f, 1.0f, 0.0f}, true);
    appendCap(-halfHeight, {0.0f, -1.0f, 0.0f}, false);
    return mesh;
}

MeshData PrimitiveGenerator::createSphere(float radius, unsigned int latitudeSegments,
                                          unsigned int longitudeSegments)
{
    requirePositive(radius, "radius");
    requireSegments(latitudeSegments, 3, "latitudeSegments");
    requireSegments(longitudeSegments, 3, "longitudeSegments");

    MeshData mesh{"Sphere"};
    const unsigned int ringCount = latitudeSegments - 1;
    mesh.vertices.reserve(2 + ringCount * longitudeSegments);
    mesh.indices.reserve(6 * longitudeSegments * (latitudeSegments - 1));

    mesh.vertices.push_back(makeVertex({0.0f, radius, 0.0f}, {0.0f, 1.0f, 0.0f},
                                       {0.5f, 1.0f}));
    for (unsigned int latitude = 1; latitude < latitudeSegments; ++latitude)
    {
        const float v = static_cast<float>(latitude) / static_cast<float>(latitudeSegments);
        const float phi = glm::pi<float>() * v;
        const float ringRadius = radius * std::sin(phi);
        const float y = radius * std::cos(phi);

        for (unsigned int longitude = 0; longitude < longitudeSegments; ++longitude)
        {
            const float u = static_cast<float>(longitude) /
                            static_cast<float>(longitudeSegments);
            const float theta = glm::two_pi<float>() * u;
            const glm::vec3 position{ringRadius * std::cos(theta), y,
                                     ringRadius * std::sin(theta)};
            mesh.vertices.push_back(makeVertex(position, glm::normalize(position), {u, 1.0f - v}));
        }
    }

    const std::uint32_t bottomPole = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(makeVertex({0.0f, -radius, 0.0f}, {0.0f, -1.0f, 0.0f},
                                       {0.5f, 0.0f}));

    const std::uint32_t firstRing = 1;
    for (unsigned int longitude = 0; longitude < longitudeSegments; ++longitude)
    {
        const std::uint32_t current = firstRing + longitude;
        const std::uint32_t next = firstRing + ((longitude + 1) % longitudeSegments);
        mesh.indices.insert(mesh.indices.end(), {0, next, current});
    }

    for (unsigned int ring = 0; ring + 1 < ringCount; ++ring)
    {
        const std::uint32_t upper = firstRing + ring * longitudeSegments;
        const std::uint32_t lower = upper + longitudeSegments;
        for (unsigned int longitude = 0; longitude < longitudeSegments; ++longitude)
        {
            const std::uint32_t nextLongitude = (longitude + 1) % longitudeSegments;
            const std::uint32_t upperCurrent = upper + longitude;
            const std::uint32_t upperNext = upper + nextLongitude;
            const std::uint32_t lowerCurrent = lower + longitude;
            const std::uint32_t lowerNext = lower + nextLongitude;
            mesh.indices.insert(mesh.indices.end(),
                                {upperCurrent, upperNext, lowerCurrent,
                                 upperNext, lowerNext, lowerCurrent});
        }
    }

    const std::uint32_t lastRing = firstRing + (ringCount - 1) * longitudeSegments;
    for (unsigned int longitude = 0; longitude < longitudeSegments; ++longitude)
    {
        const std::uint32_t current = lastRing + longitude;
        const std::uint32_t next = lastRing + ((longitude + 1) % longitudeSegments);
        mesh.indices.insert(mesh.indices.end(), {current, next, bottomPole});
    }
    return mesh;
}
