#version 450

// Fluid render: visualize the dye field as a colour ramp. Reads the dye value
// (vec4.z) from the simulation SSBO at the cell matching this fragment's UV,
// then maps it through a hot/cold palette.

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0, std430) readonly buffer FluidState {
    vec4 cells[];   // xy = velocity, z = dye, w = unused
} state;

layout(push_constant) uniform FluidRenderParams {
    uint gridSize;
} rp;

void main() {
    // Map fragment UV to a grid cell index. Nearest sampling keeps the
    // cell-resolution look (this is a benchmark, not a pretty renderer).
    uvec2 cell = uvec2(clamp(inUV * vec2(rp.gridSize), vec2(0.0), vec2(rp.gridSize - 1u)));
    uint idx = cell.y * rp.gridSize + cell.x;

    float dye = state.cells[idx].z;
    float speed = length(state.cells[idx].xy);

    // Two-channel visual: dye in warm tones, speed in cool.
    float d = clamp(dye, 0.0, 1.0);
    float s = clamp(speed * 0.5, 0.0, 1.0);
    vec3 warm = mix(vec3(0.05, 0.08, 0.13), vec3(1.0, 0.55, 0.20), d);
    vec3 cool = vec3(0.30, 0.55, 0.95) * s * 0.5;
    outColor = vec4(warm + cool, 1.0);
}
