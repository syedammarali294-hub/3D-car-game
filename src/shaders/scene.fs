#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform float fogDensity;

out vec4 finalColor;

const vec3 SUN_COLOR = vec3(1.08, 1.00, 0.88);
const vec3 SKY_AMBIENT = vec3(0.34, 0.42, 0.56);
const vec3 GROUND_BOUNCE = vec3(0.20, 0.19, 0.15);

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    vec3 n = normalize(fragNormal);
    vec3 toLight = normalize(-lightDir);
    vec3 toView = normalize(viewPos - fragPosition);

    // Half-lambert keeps shadowed sides readable rather than crushing them to black.
    float ndl = dot(n, toLight);
    float diffuse = pow(ndl * 0.5 + 0.5, 1.6);

    // Hemisphere ambient: sky light from above, warm bounce from the ground below.
    float hemi = n.y * 0.5 + 0.5;
    vec3 ambient = mix(GROUND_BOUNCE, SKY_AMBIENT, hemi);

    vec3 halfway = normalize(toLight + toView);
    float spec = pow(max(dot(n, halfway), 0.0), 56.0) * max(ndl, 0.0);

    // Rim term fakes the light wrapping around silhouettes.
    float rim = pow(1.0 - max(dot(n, toView), 0.0), 3.5) * 0.25;

    vec3 color = albedo.rgb * (SUN_COLOR * diffuse + ambient);
    color += SUN_COLOR * spec * 0.35;
    color += SKY_AMBIENT * rim;

    float dist = length(viewPos - fragPosition);
    float fog = 1.0 - exp(-fogDensity * dist);
    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));

    // Gentle filmic roll-off so bright paint and sky do not clip flat.
    color = color / (color + vec3(1.05)) * 1.9;
    color = pow(color, vec3(0.92));

    finalColor = vec4(color, albedo.a);
}
