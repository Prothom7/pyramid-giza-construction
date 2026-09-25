#include "graphics/Texture.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>

#include <glad/glad.h>

#include "graphics/PrimitiveGenerator.h"

namespace
{
std::uint32_t hash2d(std::uint32_t x, std::uint32_t y, std::uint32_t seed)
{
    std::uint32_t value = x * 374761393u + y * 668265263u + seed * 2246822519u;
    value = (value ^ (value >> 13u)) * 1274126177u;
    return value ^ (value >> 16u);
}

float noise(std::uint32_t x, std::uint32_t y, std::uint32_t seed)
{
    return static_cast<float>(hash2d(x, y, seed) & 0xffffu) / 65535.0f;
}

std::uint8_t channel(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

bool finiteUv(const MeshData& mesh)
{
    for (const Vertex& vertex : mesh.vertices)
    {
        if (!std::isfinite(vertex.texCoord.x) || !std::isfinite(vertex.texCoord.y) ||
            vertex.texCoord.x < -1.0e-5f || vertex.texCoord.x > 1.00001f ||
            vertex.texCoord.y < -1.0e-5f || vertex.texCoord.y > 1.00001f)
            return false;
    }
    return true;
}
}

TextureData TextureGenerator::generate(TextureId id, int size)
{
    if (size <= 0)
        throw std::invalid_argument("Texture size must be positive");
    TextureData data;
    data.width = size;
    data.height = size;
    data.pixels.resize(static_cast<std::size_t>(size) * size * 3u);
    const std::uint32_t seed = 41u + static_cast<std::uint32_t>(id) * 97u;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float n = noise(static_cast<std::uint32_t>(x),
                                  static_cast<std::uint32_t>(y), seed);
            float value = 0.82f + 0.16f * n;
            if (id == TextureId::Sand)
                value *= 0.93f + 0.055f * std::sin((x + y * 0.19f) * 0.16f);
            else if (id == TextureId::Limestone)
                value *= ((x / 64 + y / 48) % 2 == 0) ? 1.0f : 0.94f;
            else if (id == TextureId::QuarryStone)
                value *= 0.86f + 0.14f * noise(x / 4u, y / 4u, seed + 9u);
            else if (id == TextureId::Wood)
                value *= 0.86f + 0.12f * std::sin(x * 0.22f + n * 3.0f);
            else if (id == TextureId::Cloth)
                value *= ((x / 3 + y / 3) % 2 == 0) ? 1.0f : 0.90f;
            else if (id == TextureId::Water)
                value *= 0.88f + 0.10f * std::sin(y * 0.17f + x * 0.035f);
            else if (id == TextureId::Foliage)
                value *= 0.80f + 0.20f * noise(x / 3u, y / 3u, seed + 23u);
            else if (id == TextureId::None)
                value = 1.0f;

            const std::size_t offset =
                (static_cast<std::size_t>(y) * size + x) * 3u;
            data.pixels[offset] = channel(value);
            data.pixels[offset + 1] = channel(value * (id == TextureId::Water ? 1.04f : 1.0f));
            data.pixels[offset + 2] = channel(value * (id == TextureId::Sand ? 0.94f : 1.0f));
        }
    }
    return data;
}

Texture::~Texture()
{
    cleanup();
}

Texture::Texture(Texture&& other) noexcept : id_(other.id_)
{
    other.id_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        cleanup();
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

void Texture::upload(const TextureData& data)
{
    if (data.width <= 0 || data.height <= 0 ||
        data.pixels.size() != static_cast<std::size_t>(data.width * data.height * 3))
        throw std::invalid_argument("Invalid RGB texture data");
    cleanup();
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, data.width, data.height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, data.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::bind(unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::cleanup()
{
    if (id_ != 0)
    {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
}

void TextureLibrary::initialize()
{
    memoryBytes_ = 0;
    for (std::size_t index = 0; index < textures_.size(); ++index)
    {
        const TextureId id = static_cast<TextureId>(index);
        const int size = id == TextureId::None ? 2 : 256;
        const TextureData data = TextureGenerator::generate(id, size);
        textures_[index].upload(data);
        memoryBytes_ += data.pixels.size() * 4u / 3u;
    }
}

void TextureLibrary::bind(TextureId id, unsigned int unit) const
{
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= textures_.size())
        throw std::out_of_range("Texture id is outside the shared registry");
    textures_[index].bind(unit);
}

bool validatePhase9Textures(std::ostream& output)
{
    bool valid = true;
    std::size_t generatedBytes = 0;
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(TextureId::Count); ++index)
    {
        const TextureId id = static_cast<TextureId>(index);
        const TextureData first = TextureGenerator::generate(id, 32);
        const TextureData second = TextureGenerator::generate(id, 32);
        valid = valid && first.width == 32 && first.height == 32 &&
                first.pixels.size() == 32u * 32u * 3u &&
                first.pixels == second.pixels;
        generatedBytes += first.pixels.size();
    }

    for (std::size_t index = 0;
         index < static_cast<std::size_t>(MaterialId::Count); ++index)
    {
        const Material& material = materialDefinition(static_cast<MaterialId>(index));
        valid = valid &&
                static_cast<std::size_t>(material.texture) <
                    static_cast<std::size_t>(TextureId::Count) &&
                material.textureBlend >= 0.0f && material.textureBlend <= 1.0f &&
                material.textureScale.x > 0.0f && material.textureScale.y > 0.0f;
    }

    const MeshData triangle = PrimitiveGenerator::createTriangle();
    const MeshData plane = PrimitiveGenerator::createPlane();
    const MeshData cube = PrimitiveGenerator::createCube();
    const MeshData pyramid = PrimitiveGenerator::createPyramid();
    const MeshData disk = PrimitiveGenerator::createDisk();
    const MeshData cylinder = PrimitiveGenerator::createCylinder();
    const MeshData sphere = PrimitiveGenerator::createSphere();
    valid = valid && finiteUv(triangle) && finiteUv(plane) && finiteUv(cube) &&
            finiteUv(pyramid) && finiteUv(disk) && finiteUv(cylinder) && finiteUv(sphere);

    constexpr unsigned int longitudes = 32;
    constexpr unsigned int stride = longitudes + 1;
    for (unsigned int ring = 0; ring < 31; ++ring)
    {
        const Vertex& first = sphere.vertices[1 + ring * stride];
        const Vertex& last = sphere.vertices[1 + ring * stride + longitudes];
        valid = valid && glm::length(first.position - last.position) < 1.0e-5f &&
                std::abs(first.texCoord.x) < 1.0e-6f &&
                std::abs(last.texCoord.x - 1.0f) < 1.0e-6f;
    }

    output << "Phase 9 texture and UV validation\n"
           << "  procedural texture ids: "
           << static_cast<std::size_t>(TextureId::Count) << '\n'
           << "  deterministic sample bytes: " << generatedBytes << '\n'
           << "  sphere vertices with duplicated seam: " << sphere.vertices.size() << '\n'
           << (valid ? "Texture and UV checks passed.\n"
                     : "Texture and UV checks failed.\n");
    return valid;
}
