#version 330 core

in vec2 TexCoord;
in vec4 ParticleColor;
uniform sampler2D particleTexture;
uniform vec3 lightingTint;
out vec4 FragColor;

void main()
{
    float alpha = ParticleColor.a * texture(particleTexture, TexCoord).r;
    if (alpha < 0.005)
        discard;
    FragColor = vec4(ParticleColor.rgb * lightingTint, alpha);
}
