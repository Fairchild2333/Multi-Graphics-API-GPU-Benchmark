#version 430

// Volumetric raymarch fragment shader (OpenGL 4.3). See shaders/volumetric.frag.

in  vec2 outUV;
out vec4 outColor;

layout(binding = 1, std140) uniform VolumetricParams {
    float time;
    float stepSize;
    uint  steps;
} params;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float noise3(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);

    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);

    return mix(nxy0, nxy1, f.z) * 2.0 - 1.0;
}

float fbm(vec3 p) {
    float a = 0.5;
    float s = 0.0;
    for (int i = 0; i < 5; ++i) {
        s += a * noise3(p);
        p = p * 2.02 + vec3(1.7, 9.2, 3.3);
        a *= 0.5;
    }
    return s;
}

void main() {
    vec2 uv = (outUV - 0.5) * 2.0;
    uv.x *= 1.7777;

    float ang = params.time * 0.15;
    vec3 camPos = vec3(cos(ang) * 3.0, 1.5, sin(ang) * 3.0);
    vec3 camFwd = normalize(-camPos);
    vec3 camRight = normalize(cross(camFwd, vec3(0.0, 1.0, 0.0)));
    vec3 camUp    = cross(camRight, camFwd);

    vec3 ro = camPos;
    vec3 rd = normalize(camFwd + camRight * uv.x + camUp * uv.y);

    float density = 0.0;
    float t = 0.0;
    for (uint i = 0u; i < params.steps; ++i) {
        vec3 p = ro + rd * t;
        float w = fbm(p * 0.5 + vec3(0.0, params.time * 0.05, 0.0));
        float d = fbm(p * 0.5 + w * 0.6);
        d = smoothstep(0.0, 0.6, d + 0.3);
        density += d;
        t += params.stepSize;
    }

    float trans = exp(-density * 0.06);
    float v = clamp(density / float(params.steps) * 4.0, 0.0, 1.0);
    vec3 cold = vec3(0.45, 0.62, 0.85);
    vec3 hot  = vec3(1.00, 0.55, 0.30);
    vec3 col = mix(cold, hot, v) * (1.0 - trans) + vec3(0.05, 0.08, 0.13) * trans;
    outColor = vec4(col, 1.0);
}
