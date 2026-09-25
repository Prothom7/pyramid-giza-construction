#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform vec3 materialBaseColor;
uniform float materialAmbient;
uniform float materialDiffuse;
uniform float materialSpecular;
uniform float materialShininess;
uniform sampler2D materialTexture;
uniform vec2 materialTextureScale;
uniform vec2 materialTextureOffset;
uniform float materialTextureBlend;
uniform int texturesEnabled;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform vec3 ambientColor;
uniform float ambientIntensity;
uniform vec3 viewPosition;
uniform int lightingDebugMode;
uniform sampler2D shadowMap;
uniform int shadowsEnabled;
uniform int shadowDebugMode;
uniform float shadowMinimumBias;
uniform float shadowSlopeBias;
uniform float shadowStrength;

float calculateShadow(vec3 normal, vec3 toLight)
{
    if (shadowsEnabled == 0 || FragPosLightSpace.w <= 0.0)
        return 0.0;

    vec3 projected = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projected = projected * 0.5 + 0.5;
    if (projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0 ||
        projected.z < 0.0 || projected.z > 1.0)
        return 0.0;

    float bias = max(shadowSlopeBias * (1.0 - max(dot(normal, toLight), 0.0)),
                     shadowMinimumBias);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(shadowMap,
                                         projected.xy + vec2(x, y) * texelSize).r;
            shadow += projected.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    vec3 textureSample = texture(materialTexture,
                                 TexCoord * materialTextureScale +
                                 materialTextureOffset).rgb;
    vec3 texturedColor = materialBaseColor * textureSample * 1.12;
    vec3 surfaceColor = texturesEnabled != 0
        ? mix(materialBaseColor, texturedColor, materialTextureBlend)
        : materialBaseColor;
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

    vec3 ambient = ambientIntensity * materialAmbient * surfaceColor * ambientColor;
    vec3 diffuse = sunIntensity * materialDiffuse * nDotL * surfaceColor * sunColor;
    vec3 specular = sunIntensity * materialSpecular * blinnFactor * sunColor;
    float shadow = calculateShadow(normal, toLight);
    float visibility = 1.0 - shadow * shadowStrength;

    vec3 result;
    if (shadowDebugMode == 1)
        result = vec3(visibility);
    else if (lightingDebugMode == 1)
        result = visibility * diffuse;
    else if (lightingDebugMode == 2)
        result = visibility * specular;
    else if (lightingDebugMode == 3)
        result = normal * 0.5 + 0.5;
    else if (lightingDebugMode == 4)
        result = surfaceColor;
    else
        result = ambient + visibility * (diffuse + specular);
    FragColor = vec4(result, 1.0);
}
