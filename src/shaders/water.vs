#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform float time;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;

// Two crossing swells, evaluated per vertex so the fragment stage stays cheap.
float waveHeight(vec2 p)
{
    return sin(p.x * 0.09 + time * 1.1) * 0.22
         + sin(p.y * 0.13 - time * 0.8) * 0.16
         + sin((p.x + p.y) * 0.05 + time * 0.5) * 0.12;
}

void main()
{
    vec3 pos = vertexPosition;
    vec3 world = vec3(matModel * vec4(pos, 1.0));

    float h = waveHeight(world.xz);
    pos.y += h;

    // Analytic-ish normal from neighbouring wave samples.
    float e = 1.5;
    float hx = waveHeight(world.xz + vec2(e, 0.0));
    float hz = waveHeight(world.xz + vec2(0.0, e));
    vec3 n = normalize(vec3(h - hx, e, h - hz));

    fragPosition = vec3(matModel * vec4(pos, 1.0));
    fragTexCoord = vertexTexCoord;
    fragNormal = n;

    gl_Position = mvp * vec4(pos, 1.0);
}
