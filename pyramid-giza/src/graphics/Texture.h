#pragma once

#include <array>
#include <cstdint>
#include <iosfwd>
#include <vector>

#include "scene/SceneTypes.h"

struct TextureData
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

class TextureGenerator
{
public:
    static TextureData generate(TextureId id, int size = 256);
};

class Texture
{
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void upload(const TextureData& data);
    void bind(unsigned int unit) const;
    void cleanup();
    bool valid() const { return id_ != 0; }

private:
    unsigned int id_ = 0;
};

class TextureLibrary
{
public:
    void initialize();
    void bind(TextureId id, unsigned int unit) const;
    std::size_t memoryBytes() const { return memoryBytes_; }

private:
    std::array<Texture, static_cast<std::size_t>(TextureId::Count)> textures_;
    std::size_t memoryBytes_ = 0;
};

bool validatePhase9Textures(std::ostream& output);
