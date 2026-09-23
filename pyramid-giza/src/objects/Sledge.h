#pragma once

#include <vector>

#include "scene/SceneTypes.h"

class Sledge
{
public:
    static std::vector<ObjectPart> create(bool loadedStone);
    static constexpr std::size_t unloadedPartCount() { return 8; }
    static constexpr std::size_t loadedPartCount() { return 9; }
};
