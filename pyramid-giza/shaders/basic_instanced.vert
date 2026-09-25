#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in mat4 instanceModel;
layout (location = 7) in mat3 instanceNormalMatrix;
layout (location = 10) in vec4 instanceUvTransform;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec3 WorldPosition;
out vec3 WorldNormal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;

void main()
{
    vec4 worldPosition = instanceModel * vec4(aPosition, 1.0);
    WorldPosition = worldPosition.xyz;
    WorldNormal = normalize(instanceNormalMatrix * aNormal);
    TexCoord = aTexCoord * instanceUvTransform.xy + instanceUvTransform.zw;
    FragPosLightSpace = lightSpaceMatrix * worldPosition;
    gl_Position = projection * view * worldPosition;
}
