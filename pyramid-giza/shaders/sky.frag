#version 330 core

in vec2 ndc;
out vec4 FragColor;

uniform mat3 cameraToWorld;
uniform vec2 projectionScale;
uniform vec3 horizonColor;
uniform vec3 zenithColor;
uniform float zenithElevationScale;

void main()
{
    vec3 viewDirection = normalize(cameraToWorld *
        vec3(ndc * projectionScale, -1.0));
    float height = clamp(viewDirection.y / zenithElevationScale, 0.0, 1.0);
    float blend = height * height * (3.0 - 2.0 * height);
    FragColor = vec4(mix(horizonColor, zenithColor, blend), 1.0);
}
