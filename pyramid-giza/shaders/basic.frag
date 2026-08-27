#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

// A "sampler2D" is GLSL's type for a bound texture we can read from.
uniform sampler2D texture1;

void main()
{
    // texture() looks up the color at coordinate TexCoord in the bound image.
    FragColor = texture(texture1, TexCoord);
}