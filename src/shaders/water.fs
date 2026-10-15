#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;

uniform vec3 viewPos;

out vec4 finalColor;

const vec3 SUN_DIR = vec3(-0.45, -0.78, -0.42);
const vec3 SUN_COLOR = vec3(1.08, 1.00, 0.88);
const vec3 SKY_TINT = vec3(0.55, 0.70, 0.86);
const vec3 DEEP = vec3(0.04, 0.14, 0.22);
const vec3 SHALLOW = vec3(0.10, 0.34, 0.40);
const vec3 FOG = vec3(0.62, 0.74, 0.88);

void main()
{
    vec3 n = normalize(fragNormal);
    vec3 toView = normalize(viewPos - fragPosition);
    vec3 toLight = normalize(-SUN_DIR);

    // Grazing angles reflect the sky, steep angles show the water body colour.
    float fresnel = pow(1.0 - clamp(dot(n, toView), 0.0, 1.0), 3.0);

    vec3 body = mix(DEEP, SHALLOW, clamp(n.y, 0.0, 1.0));
    vec3 color = mix(body, SKY_TINT, clamp(fresnel * 1.25, 0.0, 1.0));

    vec3 halfway = normalize(toLight + toView);
    float glint = pow(max(dot(n, halfway), 0.0), 220.0);
    color += SUN_COLOR * glint * 1.6;

    float dist = length(viewPos - fragPosition);
    float fog = 1.0 - exp(-0.0028 * dist);
    color = mix(color, FOG, clamp(fog, 0.0, 1.0));

    color = color / (color + vec3(1.05)) * 1.9;
    color = pow(color, vec3(0.92));

    finalColor = vec4(color, 0.86);
}
