#version 330 core

layout (location = 0) in vec3 aPos;

// "Uniforms" are values set once per draw call from the CPU side (main.cpp),
// shared across every vertex processed in that call. This is how we pass
// our three matrices into the shader.
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Apply Model, then View, then Projection -- read right to left.
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}