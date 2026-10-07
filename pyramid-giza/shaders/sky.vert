#version 330 core

layout (location = 0) in vec3 aPosition;
out vec2 ndc;

void main()
{
    ndc = aPosition.xy;
    gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
