#pragma once

#include <cstddef>
#include <vector>

#include "scene/SceneTypes.h"

class Scaffold
{
public:
    static std::vector<ObjectPart> createModule();
    static constexpr float width() { return 4.60f; }
    static constexpr float depth() { return 2.40f; }
    static constexpr float levelHeight() { return 2.70f; }
    static constexpr std::size_t partCount() { return 13; }
};
