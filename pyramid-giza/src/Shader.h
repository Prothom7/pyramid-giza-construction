#pragma once

#include <string>

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use() const;

private:
    void checkCompileErrors(unsigned int shader, const std::string& type);
};