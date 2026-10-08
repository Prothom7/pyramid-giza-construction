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
uniform vec3 hazeColor;
uniform float hazeStart;
uniform float hazeEnd;
uniform float hazeMaximumBlend;
uniform int terrainHazeEnabled;
uniform vec2 terrainBoundsMin;
uniform vec2 terrainBoundsMax;
uniform vec2 terrainEdgeWidth;
uniform vec2 terrainEdgeGate;
uniform float terrainEdgeMaximumBlend;
uniform int lightingDebugMode;
uniform sampler2D shadowMap;
uniform int shadowsEnabled;
uniform int shadowDebugMode;
uniform float shadowMinimumBias;
uniform float shadowSlopeBias;
uniform float shadowStrength;
uniform int inspectionLightEnabled;
uniform vec3 inspectionLightPosition;
uniform vec3 inspectionLightColor;
uniform float inspectionLightRange;
uniform float inspectionLightIntensity;
struct FirePointLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};
uniform FirePointLight fireLights[3];

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

    vec3 inspection = vec3(0.0);
    if (inspectionLightEnabled != 0)
    {
        vec3 lightDelta = inspectionLightPosition - WorldPosition;
        float distanceToInspectionLight = length(lightDelta);
        if (distanceToInspectionLight < inspectionLightRange &&
            distanceToInspectionLight > 0.001)
        {
            vec3 localLightDirection = lightDelta / distanceToInspectionLight;
            float localDiffuse = max(dot(normal, localLightDirection), 0.0);
            float normalizedDistance = distanceToInspectionLight / inspectionLightRange;
            float attenuation = 1.0 /
                (1.0 + 2.0 * normalizedDistance + 5.0 * normalizedDistance * normalizedDistance);
            inspection = inspectionLightIntensity * attenuation *
                (0.18 + materialDiffuse * localDiffuse) * surfaceColor *
                inspectionLightColor;
        }
    }

    vec3 fireLighting = vec3(0.0);
    for (int i = 0; i < 3; ++i)
    {
        vec3 delta = fireLights[i].position - WorldPosition;
        float distanceToFire = length(delta);
        if (fireLights[i].intensity > 0.0 && distanceToFire < fireLights[i].radius)
        {
            float fraction = distanceToFire / fireLights[i].radius;
            float falloff = 1.0 - fraction * fraction * (3.0 - 2.0 * fraction);
            float diffuseFire = max(dot(normal, delta / max(distanceToFire, 0.001)), 0.0);
            fireLighting += surfaceColor * fireLights[i].color *
                fireLights[i].intensity * falloff *
                (0.15 + materialDiffuse * diffuseFire);
        }
    }

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
        result = ambient + visibility * (diffuse + specular) + inspection + fireLighting;
    // Lighting and shadowing finish before atmospheric perspective. Horizontal
    // world distance avoids over-hazing elevated camera views.
    if (lightingDebugMode == 0 && shadowDebugMode == 0)
    {
        float distanceXZ = length(viewPosition.xz - WorldPosition.xz);
        float distanceFraction = clamp((distanceXZ - hazeStart) /
                                       (hazeEnd - hazeStart), 0.0, 1.0);
        float haze = hazeMaximumBlend * distanceFraction * distanceFraction *
                     (3.0 - 2.0 * distanceFraction);
        if (terrainHazeEnabled != 0)
        {
            float edgeFraction = min(min((WorldPosition.x - terrainBoundsMin.x) /
                                         terrainEdgeWidth.x,
                                         (terrainBoundsMax.x - WorldPosition.x) /
                                         terrainEdgeWidth.x),
                                     min((WorldPosition.z - terrainBoundsMin.y) /
                                         terrainEdgeWidth.y,
                                         (terrainBoundsMax.y - WorldPosition.z) /
                                         terrainEdgeWidth.y));
            float edgeT = clamp(edgeFraction, 0.0, 1.0);
            float edgeFade = 1.0 - edgeT * edgeT * (3.0 - 2.0 * edgeT);
            float gateT = clamp((distanceXZ - terrainEdgeGate.x) /
                                (terrainEdgeGate.y - terrainEdgeGate.x), 0.0, 1.0);
            float distanceGate = gateT * gateT * (3.0 - 2.0 * gateT);
            haze = max(haze, terrainEdgeMaximumBlend * edgeFade * distanceGate);
        }
        result = mix(result, hazeColor, haze);
    }
    FragColor = vec4(result, 1.0);
}
