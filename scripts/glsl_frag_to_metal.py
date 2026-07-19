# -*- coding: utf-8 -*-
"""Rough GLSL fragment → Metal fragment helper for gpu_burn.frag."""
from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "shaders" / "gpu_burn.frag"
DST = ROOT / "shaders" / "gpu_burn.metal"

HEADER = r'''// Auto-ported from gpu_burn.frag for Metal GPU Burn (same visual/sample contract).
// Do not early-exit the fixed maxIter loop. Host passes GpuBurnParams (16 bytes).

#include <metal_stdlib>
using namespace metal;

struct GpuBurnParams {
    float time;
    float passIndex;
    uint  maxIter;
    uint  version;
};

struct BurnVtxOut {
    float4 position [[position]];
    float2 uv;
};

vertex BurnVtxOut gpuBurnVertex(uint vid [[vertex_id]]) {
    BurnVtxOut out;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    out.uv = uv;
    out.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return out;
}

'''

FOOTER = r'''
fragment float4 gpuBurnFragment(BurnVtxOut in [[stage_in]],
                                constant GpuBurnParams& params [[buffer(0)]]) {
    float2 inUV = in.uv;
    float2 fragCoord = in.position.xy;
'''


def convert_body(glsl: str) -> str:
    # Drop version / layout / main wrapper — keep helpers + translate main body.
    glsl = re.sub(r"^#version.*\n", "", glsl)
    glsl = re.sub(r"layout\(location = 0\) in\s+vec2 inUV;\s*", "", glsl)
    glsl = re.sub(r"layout\(location = 0\) out\s+vec4 outColor;\s*", "", glsl)
    glsl = re.sub(
        r"layout\(push_constant\) uniform GpuBurnV1Params \{[^}]+\} params;\s*",
        "",
        glsl,
        flags=re.S,
    )
    # Extract helpers (everything before void main)
    m = re.search(r"void main\(\)\s*\{(.*)\}\s*$", glsl, flags=re.S)
    if not m:
        raise SystemExit("main() not found")
    helpers = glsl[: m.start()]
    body = m.group(1)

    def tr(s: str) -> str:
        s = s.replace("uvec2", "uint2").replace("uvec3", "uint3").replace("uvec4", "uint4")
        s = s.replace("vec2", "float2").replace("vec3", "float3").replace("vec4", "float4")
        s = s.replace("mix(", "mix(")  # same
        s = s.replace("fract(", "fract(")
        s = s.replace("atan(", "atan2(")  # Metal atan2(y,x); GLSL atan(y,x) two-arg
        # Fix two-arg atan already atan(y,x) in GLSL → atan2(y,x) in Metal OK if we replace atan(
        # But single-arg atan(x) would break — gpu_burn only uses two-arg.
        s = re.sub(r"\batan2\(", "atan2(", s)  # after replace atan→atan2
        # Wait we replaced atan( with atan2( — good for two-arg
        s = s.replace("gl_FragCoord.xy", "fragCoord")
        s = s.replace("outColor =", "return")
        s = s.replace("discard;", "discard_fragment();")
        # const float → constant float is wrong in function; keep as float
        s = s.replace("const float", "const float")
        s = s.replace("const uint", "const uint")
        return s

    # First pass: atan → temporary, then helpers
    helpers = helpers.replace("atan(", "ATAN2(")
    body = body.replace("atan(", "ATAN2(")
    helpers = tr(helpers).replace("ATAN2(", "atan2(")
    body = tr(body).replace("ATAN2(", "atan2(")

    # Fix return vec4 → already return float4(...)
    # GLSL `return` after outColor= became return vec4 → return float4 — good
    # But `return float4(...);` needs semicolon - outColor = vec4(...); → return vec4(...);
    body = re.sub(r"return\s+(float4\([^;]+\));", r"return \1;", body)

    return helpers + "\n" + FOOTER + body + "\n}\n"


def main() -> None:
    glsl = SRC.read_text(encoding="utf-8")
    metal = HEADER + convert_body(glsl)
    DST.write_text(metal, encoding="utf-8")
    print(f"wrote {DST} ({len(metal)} bytes)")


if __name__ == "__main__":
    main()
