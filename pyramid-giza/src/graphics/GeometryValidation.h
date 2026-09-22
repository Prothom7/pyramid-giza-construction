#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "graphics/Mesh.h"

struct GeometryValidationResult
{
    bool valid = true;
    std::vector<std::string> errors;
};

GeometryValidationResult validateMeshData(const MeshData& mesh);
bool validatePrimitiveFoundation(std::ostream& output);
