#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 instanceCenterSize;
layout (location = 4) in vec4 instanceColorAlpha;
layout (location = 5) in float instanceRotation;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;

out vec2 TexCoord;
out vec4 ParticleColor;

void main()
{
    float c = cos(instanceRotation);
    float s = sin(instanceRotation);
    vec2 corner = mat2(c, -s, s, c) * aPosition.xy;
    vec3 worldPosition = instanceCenterSize.xyz
        + cameraRight * corner.x * instanceCenterSize.w
        + cameraUp * corner.y * instanceCenterSize.w;
    TexCoord = aTexCoord;
    ParticleColor = instanceColorAlpha;
    gl_Position = projection * view * vec4(worldPosition, 1.0);
}
