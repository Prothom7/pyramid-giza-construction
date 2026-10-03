#pragma once

#include <vector>

#include "scene/SceneTypes.h"

class Sledge
{
public:
    static constexpr float runnerCenterY = 0.16f;
    static constexpr float runnerHeight = 0.22f;
    static constexpr float runnerBottomLocalY = runnerCenterY - 0.5f * runnerHeight;
    static std::vector<ObjectPart> create(bool loadedStone);
    static constexpr std::size_t unloadedPartCount() { return 8; }
    static constexpr std::size_t loadedPartCount() { return 9; }
};
