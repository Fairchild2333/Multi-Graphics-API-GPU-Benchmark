// Legacy 2D Fluid (Stam stable fluids) — DX11/DX12 HLSL port.
// Single source file with four compute entry points plus a fullscreen render pass.
// Numerical logic matches the Vulkan GLSL passes in shaders/fluid_*.comp and
// shaders/fluid_render.{vert,frag} exactly.
//
// Compute UAV table (matches Vulkan descriptor set 0 bindings):
//   u0 = inCells   RWStructuredBuffer<float4>
//   u1 = outCells  RWStructuredBuffer<float4>
//   u2 = inP       RWStructuredBuffer<float>
//   u3 = outP      RWStructuredBuffer<float>
//   u4 = div       RWStructuredBuffer<float>
// All five slots are RWStructuredBuffer so DX12 root signatures can expose a
// fixed 5-UAV table even when a pass only reads some buffers.
//
// Render graphics path (DX12):
//   t0 = cells     StructuredBuffer<float4>  (SRV — read-only dye/velocity state)
//   b0 = FluidRenderParams cbuffer
//
// Note: FluidRenderParams members use rp_* identifiers because gridSize/time
// would collide with FluidParams in this single translation unit. Offsets match
// gpu_common.h FluidRenderParams exactly: gridSize@0, time@4, exposure@8,
// version@12.

cbuffer FluidParams : register(b0) {
    float dt;
    float dx;
    uint  gridSize;
    float time;
};

RWStructuredBuffer<float4> inCells  : register(u0);
RWStructuredBuffer<float4> outCells : register(u1);
RWStructuredBuffer<float>  inP     : register(u2);
RWStructuredBuffer<float>  outP    : register(u3);
RWStructuredBuffer<float>  div     : register(u4);

uint CellIdx(uint2 c) {
    return c.y * gridSize + c.x;
}

uint2 ClampCoord(uint2 c) {
    return min(max(c, uint2(0, 0)), uint2(gridSize - 1, gridSize - 1));
}

// Pass 1/4: semi-Lagrangian advection + deterministic dual-vortex emitters.
[numthreads(16, 16, 1)]
void CSAdvect(uint3 DTid : SV_DispatchThreadID) {
    uint2 c = DTid.xy;
    if (c.x >= gridSize || c.y >= gridSize)
        return;

    float2 vel = inCells[CellIdx(c)].xy;
    float2 pos = float2(c) - vel * dt / dx;
    pos = clamp(pos, float2(0.0, 0.0), float2(gridSize - 1, gridSize - 1));

    uint2 i0 = uint2(floor(pos));
    float2 f = pos - float2(i0);
    uint2 i1 = i0 + uint2(1, 1);
    i0 = ClampCoord(i0);
    i1 = ClampCoord(i1);

    float4 v00 = inCells[CellIdx(i0)];
    float4 v10 = inCells[CellIdx(uint2(i1.x, i0.y))];
    float4 v01 = inCells[CellIdx(uint2(i0.x, i1.y))];
    float4 v11 = inCells[CellIdx(i1)];

    float4 a = lerp(v00, v10, f.x);
    float4 b = lerp(v01, v11, f.x);
    float4 fluid = lerp(a, b, f.y);

    fluid.xy *= exp(-0.22 * dt);
    fluid.zw *= exp(-0.72 * dt);

    float2 q = (float2(c) + 0.5) / float(gridSize) - 0.5;
    float phase = time * 0.72;
    float2 emitterA = float2(-0.40, -0.025 + 0.018 * sin(phase));
    float2 emitterB = float2( 0.40,  0.025 - 0.018 * sin(phase * 0.91 + 1.2));
    float2 impact = float2(0.025 * sin(phase * 0.43),
                           0.018 * cos(phase * 0.37));
    float2 da = q - emitterA;
    float2 db = q - emitterB;
    float jetA = exp(-dot(da, da) * 720.0);
    float jetB = exp(-dot(db, db) * 720.0);

    float2 forceA = normalize(impact - emitterA) * 0.88;
    float2 forceB = normalize(impact - emitterB) * 0.88;

    float r2 = dot(q, q);
    float vortexWeight = exp(-r2 * 65.0);
    float2 vortex = float2(-q.y, q.x) *
                    (0.26 + 0.05 * sin(phase * 0.63)) * vortexWeight;
    float2 breathing = (impact - q) * (0.11 + 0.03 * sin(phase * 0.47)) *
                       exp(-r2 * 12.0);
    float2 eddyA = float2(cos(q.y * 43.0 - phase * 1.37),
                          sin(q.x * 39.0 + phase * 1.11)) * 0.035;
    float2 eddyB = float2(sin(q.y * 67.0 + phase * 0.83),
                         -sin(q.x * 55.0 - phase * 0.96)) * 0.045;
    float2 eddies = (eddyA + eddyB) * exp(-r2 * 13.0);

    fluid.xy += (jetA * forceA + jetB * forceB + vortex + breathing + eddies) * dt;
    float speed = length(fluid.xy);
    if (speed > 1.0)
        fluid.xy *= 1.0 / speed;
    fluid.z = min(fluid.z + jetA * (3.8 * dt), 1.8);
    fluid.w = min(fluid.w + jetB * (3.8 * dt), 1.8);

    if (c.x == 0 || c.y == 0 || c.x == gridSize - 1 || c.y == gridSize - 1)
        fluid.xy = float2(0.0, 0.0);

    outCells[CellIdx(c)] = fluid;
}

// Pass 2/4: central-difference divergence of the advected velocity field.
[numthreads(16, 16, 1)]
void CSDivergence(uint3 DTid : SV_DispatchThreadID) {
    uint2 c = DTid.xy;
    if (c.x >= gridSize || c.y >= gridSize)
        return;

    if (c.x == 0 || c.y == 0 || c.x == gridSize - 1 || c.y == gridSize - 1) {
        div[CellIdx(c)] = 0.0;
        return;
    }

    float4 L = inCells[CellIdx(c - uint2(1, 0))];
    float4 R = inCells[CellIdx(c + uint2(1, 0))];
    float4 D = inCells[CellIdx(c - uint2(0, 1))];
    float4 U = inCells[CellIdx(c + uint2(0, 1))];

    float divergence = 0.5 * ((R.x - L.x) + (U.y - D.y)) / dx;
    div[CellIdx(c)] = divergence;
}

// Pass 3/4: one Jacobi pressure relaxation iteration.
[numthreads(16, 16, 1)]
void CSJacobi(uint3 DTid : SV_DispatchThreadID) {
    uint2 c = DTid.xy;
    if (c.x >= gridSize || c.y >= gridSize)
        return;

    if (c.x == 0 || c.y == 0 || c.x == gridSize - 1 || c.y == gridSize - 1) {
        outP[CellIdx(c)] = 0.0;
        return;
    }

    float pL = inP[CellIdx(c - uint2(1, 0))];
    float pR = inP[CellIdx(c + uint2(1, 0))];
    float pD = inP[CellIdx(c - uint2(0, 1))];
    float pU = inP[CellIdx(c + uint2(0, 1))];
    float d  = div[CellIdx(c)];

    outP[CellIdx(c)] = (pL + pR + pD + pU - d * dx * dx) * 0.25;
}

// Pass 4/4: subtract pressure gradient from advected velocity; preserve dye.
[numthreads(16, 16, 1)]
void CSSubtract(uint3 DTid : SV_DispatchThreadID) {
    uint2 c = DTid.xy;
    if (c.x >= gridSize || c.y >= gridSize)
        return;

    if (c.x == 0 || c.y == 0 || c.x == gridSize - 1 || c.y == gridSize - 1) {
        float4 edge = inCells[CellIdx(c)];
        edge.xy = float2(0.0, 0.0);
        outCells[CellIdx(c)] = edge;
        return;
    }

    float pL = inP[CellIdx(c - uint2(1, 0))];
    float pR = inP[CellIdx(c + uint2(1, 0))];
    float pD = inP[CellIdx(c - uint2(0, 1))];
    float pU = inP[CellIdx(c + uint2(0, 1))];

    float2 gradP = 0.5 * float2(pR - pL, pU - pD) / dx;

    float4 src = inCells[CellIdx(c)];
    src.xy -= gradP;
    outCells[CellIdx(c)] = src;
}

// ---- Fullscreen render (Cinematic Fluid v1 presentation) ----------------

cbuffer FluidRenderParams : register(b0) {
    uint  rp_gridSize;   // FluidRenderParams.gridSize
    float rp_time;       // FluidRenderParams.time
    float rp_exposure;   // FluidRenderParams.exposure
    uint  rp_version;    // FluidRenderParams.version
};

// DX12 graphics: read simulation state through an SRV at t0 (not a UAV).
StructuredBuffer<float4> cells : register(t0);

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID) {
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.uv  = uv;
    o.pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

uint RenderCellIndex(int2 c) {
    int2 hi = int2(rp_gridSize - 1, rp_gridSize - 1);
    int2 cc = clamp(c, int2(0, 0), hi);
    return (uint)cc.y * rp_gridSize + (uint)cc.x;
}

float4 SampleFluid(float2 uv) {
    float2 grid = clamp(uv, float2(0.0, 0.0), float2(1.0, 1.0)) *
                  float(rp_gridSize - 1);
    int2 lo = int2(floor(grid));
    int2 hi = lo + int2(1, 1);
    float2 f = frac(grid);
    float4 a = lerp(cells[RenderCellIndex(lo)],
                    cells[RenderCellIndex(int2(hi.x, lo.y))], f.x);
    float4 b = lerp(cells[RenderCellIndex(int2(lo.x, hi.y))],
                    cells[RenderCellIndex(hi)], f.x);
    return lerp(a, b, f.y);
}

float DensityAt(float2 uv) {
    float2 pigment = max(SampleFluid(uv).zw, float2(0.0, 0.0));
    return pigment.x + pigment.y;
}

float Hash21(float2 p) {
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float4 PSMain(VSOut input) : SV_TARGET {
    float2 screen = input.uv * 2.0 - 1.0;
    float cameraDrift = 0.010 * sin(rp_time * 0.17);
    float2 uv = float2(0.5 + screen.x * 0.54 + cameraDrift,
                       0.5 + screen.y * 0.47);

    float4 fluid = SampleFluid(uv);
    float2 dye = max(fluid.zw, float2(0.0, 0.0));
    float density = dye.x + dye.y;
    float mixAmount = dye.y / max(density, 1e-4);
    float speed = length(fluid.xy);

    float2 texel = float2(1.0 / float(rp_gridSize), 1.0 / float(rp_gridSize));
    float4 sampleL = SampleFluid(uv - float2(texel.x, 0.0));
    float4 sampleR = SampleFluid(uv + float2(texel.x, 0.0));
    float4 sampleD = SampleFluid(uv - float2(0.0, texel.y));
    float4 sampleU = SampleFluid(uv + float2(0.0, texel.y));
    float hL = max(sampleL.z, 0.0) + max(sampleL.w, 0.0);
    float hR = max(sampleR.z, 0.0) + max(sampleR.w, 0.0);
    float hD = max(sampleD.z, 0.0) + max(sampleD.w, 0.0);
    float hU = max(sampleU.z, 0.0) + max(sampleU.w, 0.0);
    float3 normal = normalize(float3((hL - hR) * 2.8,
                                     (hD - hU) * 2.8, 0.32));

    float2 glowStep = texel * 3.5;
    float glow = 0.0;
    glow += DensityAt(uv + glowStep * float2( 1.0,  0.0));
    glow += DensityAt(uv + glowStep * float2(-1.0,  0.0));
    glow += DensityAt(uv + glowStep * float2( 0.0,  1.0));
    glow += DensityAt(uv + glowStep * float2( 0.0, -1.0));
    glow += DensityAt(uv + glowStep * float2( 0.707,  0.707));
    glow += DensityAt(uv + glowStep * float2(-0.707,  0.707));
    glow += DensityAt(uv + glowStep * float2( 0.707, -0.707));
    glow += DensityAt(uv + glowStep * float2(-0.707, -0.707));
    glow *= 0.125;

    float3 cyan    = float3(0.008, 0.30, 1.00);
    float3 magenta = float3(0.92, 0.012, 0.38);
    float3 gold    = float3(1.00, 0.27, 0.018);
    float3 hue = lerp(cyan, lerp(magenta, gold, 0.18 + 0.30 * dye.x), mixAmount);
    float visible = smoothstep(0.10, 0.30, density);
    float body = visible * (0.32 + 0.68 * (1.0 - exp(-density * 1.20)));

    float3 lightDir = normalize(float3(-0.42, 0.58, 0.70));
    float diffuse = 0.34 + 0.66 * max(dot(normal, lightDir), 0.0);
    float rim = pow(1.0 - max(normal.z, 0.0), 2.4);
    float collision = 4.0 * min(dye.x, dye.y) / max(density, 1e-4);
    float gradientEdge = clamp(length(float2(hL - hR, hD - hU)) * 3.0, 0.0, 1.0);

    float curl = abs((sampleR.y - sampleL.y) - (sampleU.x - sampleD.x)) *
               (0.5 * float(rp_gridSize));
    float strain = length(float2(sampleR.x - sampleL.x,
                                 sampleU.y - sampleD.y)) *
                   (0.5 * float(rp_gridSize));
    float shearRidge = smoothstep(0.10, 0.85, curl + strain);
    float microRipple = 0.5 + 0.5 * sin(dot(uv + fluid.xy * 0.04,
                                            float2(71.0, -59.0)) + rp_time * 0.25);
    float filaments = 0.84 + 0.13 * shearRidge + 0.03 * microRipple;

    float3 background = lerp(float3(0.0003, 0.0008, 0.0030),
                             float3(0.0010, 0.0030, 0.0100),
                             0.5 + 0.5 * screen.y);
    float2 starGrid = input.uv * float2(320.0, 180.0);
    float2 starCell = floor(starGrid);
    float2 starLocal = frac(starGrid) - 0.5;
    float starSeed = Hash21(starCell);
    float star = smoothstep(0.998, 1.0, starSeed) *
                 (1.0 - smoothstep(0.035, 0.18, length(starLocal))) *
                 (0.55 + 0.45 * sin(rp_time * 0.7 + starSeed * 31.0));
    background += star * float3(0.22, 0.34, 0.58);

    float3 color = background;
    float glowVisible = smoothstep(0.08, 0.24, glow);
    float glowBody = glowVisible * (1.0 - exp(-max(glow - 0.06, 0.0) * 0.50));
    color += hue * body * filaments * (0.30 + 0.62 * diffuse);
    color += lerp(cyan, magenta, mixAmount) * glowBody * 0.12;
    color += float3(1.00, 0.24, 0.035) * collision * body * 0.14;
    color += lerp(float3(0.04, 0.35, 1.0), float3(1.0, 0.04, 0.30), mixAmount) *
             gradientEdge * body * 0.24;
    color += float3(0.12, 0.28, 0.72) * rim * body * 0.15;
    color += hue * min(speed * 1.5, 1.0) * body * 0.08;

    float vignette = clamp(1.0 - 0.34 * dot(screen * float2(0.72, 1.0),
                                           screen * float2(0.72, 1.0)), 0.30, 1.0);
    color *= vignette;
    float bars = 1.0 - smoothstep(0.93, 1.0, abs(screen.y));
    color *= bars;

    color = float3(1.0, 1.0, 1.0) - exp(-max(color, float3(0.0, 0.0, 0.0)) * rp_exposure);
    color = pow(max(color, float3(0.0, 0.0, 0.0)), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return float4(color, 1.0);
}
