#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec3 WorldPosition;
out vec3 WorldNormal;
out vec2 TexCoord;

void main()
{
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    WorldPosition = worldPosition.xyz;
    // inverse-transpose normalMatrix keeps this world-space normal correct under
    // the scene's extensive non-uniform scaling. The fragment stage normalizes
    // again after interpolation.
    WorldNormal = normalize(normalMatrix * aNormal);
    TexCoord = aTexCoord;
    gl_Position = projection * view * worldPosition;
}
