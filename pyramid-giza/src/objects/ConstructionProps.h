#pragma once

#include <vector>

#include "scene/SceneTypes.h"

class ConstructionProps
{
public:
    static std::vector<ObjectPart> createLever();
    static std::vector<ObjectPart> createMallet();
    static std::vector<ObjectPart> createWoodenFrame();
    static std::vector<ObjectPart> createRoller();
};
