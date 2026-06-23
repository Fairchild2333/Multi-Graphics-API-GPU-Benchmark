#pragma once

// Minimal column-major 4x4 matrix helpers for the Render3D camera, shared by
// all backends. Column-major (m[col*4 + row]) matches GLSL/HLSL(column_major)/MSL
// constant-buffer packing, so the same float[16] can be uploaded to every API.
//
// Clip-space conventions differ per API and are passed explicitly:
//   Vulkan : flipY = true,  zZeroToOne = true
//   DX12/11: flipY = false, zZeroToOne = true
//   Metal  : flipY = false, zZeroToOne = true
//   OpenGL : flipY = false, zZeroToOne = false   (z in [-1,1])

#include <cmath>

namespace gpu_bench {

struct V3 { float x, y, z; };

inline V3    v3sub(V3 a, V3 b)   { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline float v3dot(V3 a, V3 b)   { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3    v3cross(V3 a, V3 b) { return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x }; }
inline V3    v3norm(V3 a)        { float l = std::sqrt(v3dot(a, a)); return l > 0 ? V3{a.x/l, a.y/l, a.z/l} : a; }

// C = A * B (both column-major)
inline void mat4mul(const float* A, const float* B, float* C) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += A[k*4 + r] * B[c*4 + k];
            C[c*4 + r] = s;
        }
}

inline void mat4perspective(float fovY, float aspect, float n, float f,
                            float* m, bool flipY, bool zZeroToOne) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    float t = 1.0f / std::tan(fovY * 0.5f);
    m[0]  = t / aspect;
    m[5]  = flipY ? -t : t;
    m[11] = -1.0f;
    if (zZeroToOne) {
        m[10] = f / (n - f);
        m[14] = (f * n) / (n - f);
    } else {                     // OpenGL z in [-1,1]
        m[10] = (f + n) / (n - f);
        m[14] = (2.0f * f * n) / (n - f);
    }
}

// View matrix (column-major) + world-space camera right/up for billboarding.
inline void mat4lookAt(V3 eye, V3 center, V3 wup, float* m, V3& right, V3& upOut) {
    V3 fwd = v3norm(v3sub(center, eye));
    right  = v3norm(v3cross(fwd, wup));
    upOut  = v3cross(right, fwd);
    V3 z   = { -fwd.x, -fwd.y, -fwd.z };
    m[0]=right.x; m[1]=upOut.x; m[2]=z.x; m[3]=0;
    m[4]=right.y; m[5]=upOut.y; m[6]=z.y; m[7]=0;
    m[8]=right.z; m[9]=upOut.z; m[10]=z.z; m[11]=0;
    m[12]=-v3dot(right,eye); m[13]=-v3dot(upOut,eye); m[14]=-v3dot(z,eye); m[15]=1;
}

// Build the full viewProj + camera basis for an orbiting camera at angle `t`.
// Caller supplies the API clip convention.
inline void render3dCamera(float t, float aspect, bool flipY, bool zZeroToOne,
                           float viewProj[16], float camRight[4], float camUp[4]) {
    const float R = 3.0f;
    V3 eye{ R * std::cos(t * 0.5f), 1.5f, R * std::sin(t * 0.5f) };
    float proj[16], view[16];
    V3 right{}, up{};
    mat4perspective(1.0472f /* 60deg */, aspect, 0.05f, 100.0f, proj, flipY, zZeroToOne);
    mat4lookAt(eye, V3{0,0,0}, V3{0,1,0}, view, right, up);
    mat4mul(proj, view, viewProj);
    camRight[0]=right.x; camRight[1]=right.y; camRight[2]=right.z; camRight[3]=0;
    camUp[0]=up.x; camUp[1]=up.y; camUp[2]=up.z; camUp[3]=0;
}

}  // namespace gpu_bench
