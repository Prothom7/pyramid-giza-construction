#include "Shader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

std::string Shader::readFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Could not open shader file: " + path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

unsigned int Shader::compile(unsigned int type, const std::string& source,
                             const std::string& label)
{
    const unsigned int shader = glCreateShader(type);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(logLength + 1));
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error(label + " shader compilation failed:\n" + log.data());
    }
    return shader;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    const unsigned int vertex = compile(GL_VERTEX_SHADER, readFile(vertexPath), "Vertex");
    unsigned int fragment = 0;
    try
    {
        fragment = compile(GL_FRAGMENT_SHADER, readFile(fragmentPath), "Fragment");
        id_ = glCreateProgram();
        glAttachShader(id_, vertex);
        glAttachShader(id_, fragment);
        glLinkProgram(id_);

        int success = 0;
        glGetProgramiv(id_, GL_LINK_STATUS, &success);
        if (success == GL_FALSE)
        {
            int logLength = 0;
            glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> log(static_cast<std::size_t>(logLength + 1));
            glGetProgramInfoLog(id_, logLength, nullptr, log.data());
            throw std::runtime_error("Shader program linking failed:\n" + std::string(log.data()));
        }
    }
    catch (...)
    {
        glDeleteShader(vertex);
        if (fragment != 0)
            glDeleteShader(fragment);
        if (id_ != 0)
            glDeleteProgram(id_);
        id_ = 0;
        throw;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader()
{
    if (id_ != 0)
        glDeleteProgram(id_);
}

void Shader::use() const
{
    glUseProgram(id_);
}

int Shader::uniformLocation(const std::string& name) const
{
    const auto cached = uniformLocations_.find(name);
    if (cached != uniformLocations_.end())
        return cached->second;

    const int location = glGetUniformLocation(id_, name.c_str());
    if (location < 0)
        throw std::runtime_error("Shader uniform is missing or inactive: " + name);
    uniformLocations_.emplace(name, location);
    return location;
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat3(const std::string& name, const glm::mat3& value) const
{
    glUniformMatrix3fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(uniformLocation(name), value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(uniformLocation(name), value);
}
