#version 330 core

in vec2 ndc;
out vec4 FragColor;

uniform mat3 cameraToWorld;
uniform vec2 projectionScale;
uniform vec3 horizonColor;
uniform vec3 zenithColor;
uniform float zenithElevationScale;
uniform float nightFactor;
uniform vec3 moonDirection;

float starHash(vec2 cell)
{
    return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

float stars(vec3 direction)
{
    vec2 skyCell = vec2(atan(direction.z, direction.x) * 100.0,
                        asin(clamp(direction.y, -1.0, 1.0)) * 80.0);
    vec2 cell = floor(skyCell);
    vec2 point = fract(skyCell) - vec2(0.5);
    vec2 offset = vec2(starHash(cell + 17.0), starHash(cell + 43.0));
    float star = step(0.997, starHash(cell));
    star *= 1.0 - smoothstep(0.025, 0.11,
                            length(point - (offset - 0.5) * 0.45));
    return star * smoothstep(0.01, 0.12, direction.y);
}

void main()
{
    vec3 viewDirection = normalize(cameraToWorld *
        vec3(ndc * projectionScale, -1.0));
    float height = clamp(viewDirection.y / zenithElevationScale, 0.0, 1.0);
    float blend = height * height * (3.0 - 2.0 * height);
    vec3 color = mix(horizonColor, zenithColor, blend);
    float moonAlignment = dot(viewDirection, normalize(moonDirection));
    float moonDisc = smoothstep(cos(0.019), cos(0.012), moonAlignment);
    float moonHalo = 0.08 * pow(max(moonAlignment, 0.0), 512.0);
    color = mix(color, vec3(0.70, 0.73, 0.79), nightFactor * moonDisc);
    color += nightFactor * (0.32 * stars(viewDirection) + moonHalo);
    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
