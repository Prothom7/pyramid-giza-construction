#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 3) in mat4 instanceModel;

uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * instanceModel * vec4(aPosition, 1.0);
}
