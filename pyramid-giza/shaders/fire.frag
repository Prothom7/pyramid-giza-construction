#version 330 core
in vec2 uv;
out vec4 FragColor;

uniform float timeSeconds;
uniform float phase;
uniform float opacity;
uniform int effectKind; // 0 outer flame, 1 inner flame, 2 ember, 3 smoke

void main()
{
    vec2 p = uv - vec2(0.5, 0.0);
    if (effectKind == 2)
    {
        float a = (1.0 - smoothstep(0.22, 0.48, length(uv - 0.5))) * opacity;
        FragColor = vec4(1.0, 0.62, 0.25, a);
        return;
    }
    if (effectKind == 3)
    {
        float a = (1.0 - smoothstep(0.22, 0.51, length(uv - 0.5))) * opacity;
        FragColor = vec4(0.39, 0.37, 0.34, a);
        return;
    }
    float y = clamp(uv.y, 0.0, 1.0);
    float turbulence = 0.08 * sin(12.0 * y + 8.4 * timeSeconds + phase) +
                       0.045 * sin(27.0 * y - 14.7 * timeSeconds + phase * 2.1) +
                       0.022 * sin(43.0 * y + 22.1 * timeSeconds + phase * 3.8);
    float center = 0.5 + turbulence * y * y;
    float width = (effectKind == 1 ? 0.22 : 0.39) *
                  pow(max(0.0, 1.0 - y), effectKind == 1 ? 0.70 : 0.52);
    width *= 0.9 + 0.1 * sin(9.0 * timeSeconds + phase + 19.0 * y);
    float a = (1.0 - smoothstep(width * 0.68, width, abs(uv.x - center))) *
              smoothstep(0.0, 0.10, y) * (1.0 - smoothstep(0.77, 1.0, y));
    a *= opacity;
    vec3 outer = mix(vec3(0.70, 0.19, 0.05), vec3(1.0, 0.52, 0.13), y);
    vec3 inner = mix(vec3(1.0, 0.73, 0.28), vec3(1.0, 0.89, 0.58), y);
    FragColor = vec4(effectKind == 1 ? inner : outer, a);
}
