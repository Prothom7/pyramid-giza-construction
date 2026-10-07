#version 330 core
in vec2 cardUv;
out vec4 FragColor;

uniform vec3 cloudTint;
uniform float cloudOpacity;
uniform float shapeSeed;

float lobe(vec2 uv, vec2 center, vec2 radius)
{
    float distanceFromCenter = length((uv - center) / radius);
    return 1.0 - smoothstep(0.65, 1.0, distanceFromCenter);
}

void main()
{
    float shift = (shapeSeed - 0.5) * 0.10;
    vec2 warped = cardUv + 0.018 * vec2(
        sin(15.0 * cardUv.y + 11.0 * cardUv.x + shapeSeed * 17.0),
        sin(17.0 * cardUv.x - 9.0 * cardUv.y + shapeSeed * 13.0));
    float body = lobe(warped, vec2(0.33 + shift, 0.43), vec2(0.27, 0.26));
    body = max(body, lobe(warped, vec2(0.53, 0.52), vec2(0.25, 0.32)));
    body = max(body, lobe(warped, vec2(0.70 - shift, 0.43), vec2(0.20, 0.24)));
    body = max(body, lobe(warped, vec2(0.23, 0.40 + shift), vec2(0.14, 0.16)));
    body = max(body, 0.7 * lobe(warped, vec2(0.59 + shift, 0.66), vec2(0.13, 0.15)));
    // Card borders fade fully; there are no rectangular silhouette edges.
    float edge = min(min(cardUv.x, 1.0 - cardUv.x),
                     min(cardUv.y, 1.0 - cardUv.y));
    float alpha = cloudOpacity * body * smoothstep(0.0, 0.09, edge);
    alpha *= 0.89 + 0.11 * sin(13.0 * warped.x + shapeSeed * 9.0) *
                            sin(11.0 * warped.y - shapeSeed * 6.0);
    vec3 color = cloudTint * mix(0.86, 1.0, cardUv.y);
    FragColor = vec4(color, alpha);
}
