#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cardCenter;
uniform vec3 cameraRight;
uniform vec2 cardSize;
uniform float timeSeconds;
uniform float phase;
uniform int effectKind;
out vec2 uv;

void main()
{
    uv = aTexCoord;
    float lift = max(aTexCoord.y, 0.0);
    // Three frequencies disturb the tip more than the anchored base.
    float sway = (0.11 * sin(5.1 * timeSeconds + phase + lift * 7.0) +
                  0.055 * sin(11.7 * timeSeconds + phase * 2.4 + lift * 13.0) +
                  0.025 * sin(23.9 * timeSeconds + phase * 3.1)) * lift * lift;
    if (effectKind != 0) sway = 0.0;
    vec3 world = cardCenter + cameraRight * (aPosition.x * cardSize.x + sway)
                            + vec3(0.0, aPosition.y * cardSize.y, 0.0);
    gl_Position = projection * view * vec4(world, 1.0);
}
