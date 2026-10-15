#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform float fogDensity;
uniform int desertMode;

out vec4 finalColor;

const vec3 SUN_COLOR = vec3(1.08, 1.00, 0.88);
const vec3 SKY_AMBIENT = vec3(0.34, 0.42, 0.56);
const vec3 GROUND_BOUNCE = vec3(0.20, 0.19, 0.15);

const vec3 SAND = vec3(0.76, 0.69, 0.48);
const vec3 GRASS_LOW = vec3(0.29, 0.44, 0.20);
const vec3 GRASS_HIGH = vec3(0.38, 0.50, 0.25);
const vec3 ROCK = vec3(0.42, 0.40, 0.38);

const vec3 DUNE_LOW = vec3(0.80, 0.66, 0.42);
const vec3 DUNE_HIGH = vec3(0.90, 0.76, 0.50);
const vec3 DUNE_ROCK = vec3(0.55, 0.44, 0.32);

// Cheap value noise for breaking up flat colour bands.
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main()
{
    vec3 n = normalize(fragNormal);
    float height = fragPosition.y;
    float slope = 1.0 - n.y;

    float grain = noise(fragPosition.xz * 0.08) * 0.14 + noise(fragPosition.xz * 0.4) * 0.06;

    vec3 albedo;
    if (desertMode == 1) {
        albedo = mix(DUNE_LOW, DUNE_HIGH, clamp(height / 12.0 + grain, 0.0, 1.0));
        albedo = mix(albedo, DUNE_ROCK, smoothstep(0.20, 0.48, slope)); // exposed rock on steep dune faces
    } else {
        albedo = mix(GRASS_LOW, GRASS_HIGH, clamp(height / 16.0 + grain, 0.0, 1.0));
        albedo = mix(albedo, SAND, smoothstep(2.4, 0.2, height));      // beach near the waterline
        albedo = mix(albedo, ROCK, smoothstep(0.14, 0.40, slope));     // exposed rock on steep faces
    }
    albedo *= 0.92 + grain * 0.6;

    vec3 toLight = normalize(-lightDir);
    vec3 toView = normalize(viewPos - fragPosition);

    float ndl = dot(n, toLight);
    float diffuse = pow(ndl * 0.5 + 0.5, 1.6);

    float hemi = n.y * 0.5 + 0.5;
    vec3 ambient = mix(GROUND_BOUNCE, SKY_AMBIENT, hemi);

    vec3 color = albedo * (SUN_COLOR * diffuse + ambient);

    // Wet, darker sand right at the shoreline -- skipped in the desert,
    // which has no shoreline to speak of.
    if (desertMode == 0) {
        float wet = smoothstep(1.2, -0.4, height);
        color = mix(color, color * 0.62, wet);
    }

    float dist = length(viewPos - fragPosition);
    float fog = 1.0 - exp(-fogDensity * dist);
    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));

    color = color / (color + vec3(1.05)) * 1.9;
    color = pow(color, vec3(0.92));

    finalColor = vec4(color, 1.0);
}
