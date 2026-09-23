#version 330 core

in vec3 WorldPosition;
in vec3 WorldNormal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 objectColor;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 viewPosition;
uniform float materialAmbient;
uniform float materialDiffuse;
uniform float materialSpecular;

void main()
{
    vec3 normal = normalize(WorldNormal);
    vec3 toLight = normalize(-lightDirection);

    float diffuseStrength = max(dot(normal, toLight), 0.0);
    vec3 viewDirection = normalize(viewPosition - WorldPosition);
    vec3 halfwayDirection = normalize(toLight + viewDirection);
    float specularStrength = pow(max(dot(normal, halfwayDirection), 0.0), 32.0);

    vec3 ambient = materialAmbient * objectColor;
    vec3 diffuse = materialDiffuse * diffuseStrength * objectColor * lightColor;
    vec3 specular = materialSpecular * specularStrength * lightColor;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
