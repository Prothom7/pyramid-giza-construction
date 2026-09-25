#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

// CPU-side mesh data stays independent of OpenGL so it can be generated and
// validated without a graphics context.
struct MeshData
{
    explicit MeshData(std::string meshName) : name(std::move(meshName)) {}

    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

class Mesh
{
public:
    Mesh() = default;
    explicit Mesh(const MeshData& data);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const MeshData& data);
    void draw() const;
    void bind() const;
    void cleanup();

    bool isUploaded() const { return vao_ != 0; }
    std::uint32_t indexCount() const { return indexCount_; }

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    std::uint32_t indexCount_ = 0;
};
