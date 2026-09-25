#pragma once

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

class Shader
{
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    void setMat3(const std::string& name, const glm::mat3& value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setFloat(const std::string& name, float value) const;
    void setInt(const std::string& name, int value) const;
    unsigned int id() const { return id_; }

private:
    unsigned int id_ = 0;
    mutable std::unordered_map<std::string, int> uniformLocations_;

    static std::string readFile(const std::string& path);
    static unsigned int compile(unsigned int type, const std::string& source,
                                const std::string& label);
    int uniformLocation(const std::string& name) const;
};
