#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec3 cloudCenter;
uniform vec2 cloudSize;
out vec2 cardUv;

void main()
{
    cardUv = aTexCoord;
    vec3 world = cloudCenter + cameraRight * (aPosition.x * cloudSize.x)
                             + cameraUp * (aPosition.y * cloudSize.y);
    gl_Position = projection * view * vec4(world, 1.0);
}
