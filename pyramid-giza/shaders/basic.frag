#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 materialBaseColor;
uniform float materialAmbient;
uniform float materialDiffuse;
uniform float materialSpecular;
uniform float materialShininess;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform vec3 ambientColor;
uniform float ambientIntensity;
uniform vec3 viewPosition;
uniform int lightingDebugMode;

void main()
{
    vec3 normal = normalize(WorldNormal);
    // All lighting vectors are world-space. sunDirection is the direction in
    // which sunlight rays travel, so L points in the opposite direction.
    vec3 toLight = normalize(-sunDirection);

    float nDotL = max(dot(normal, toLight), 0.0);
    vec3 viewDirection = normalize(viewPosition - WorldPosition);
    vec3 halfwayDirection = normalize(toLight + viewDirection);
    float blinnFactor = nDotL > 0.0
        ? pow(max(dot(normal, halfwayDirection), 0.0), materialShininess)
        : 0.0;

    vec3 ambient = ambientIntensity * materialAmbient * materialBaseColor * ambientColor;
    vec3 diffuse = sunIntensity * materialDiffuse * nDotL * materialBaseColor * sunColor;
    vec3 specular = sunIntensity * materialSpecular * blinnFactor * sunColor;

    vec3 result;
    if (lightingDebugMode == 1)
        result = diffuse;
    else if (lightingDebugMode == 2)
        result = specular;
    else if (lightingDebugMode == 3)
        result = normal * 0.5 + 0.5;
    else if (lightingDebugMode == 4)
        result = materialBaseColor;
    else
        result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
