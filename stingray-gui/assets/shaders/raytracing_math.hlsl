#ifndef RAYTRACING_MATH_HLSL
#define RAYTRACING_MATH_HLSL

#include "raytracing_types.hlsl"

float3 barycentricLerp(in float3 v0, in float3 v1, in float3 v2, in float3 barycentrics) {
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

Vertex barycentricLerp(in Vertex v0, in Vertex v1, in Vertex v2, in float3 barycentrics) {
    Vertex vtx;
    vtx.position = barycentricLerp(v0.position, v1.position, v2.position, barycentrics);
    vtx.normal = normalize(barycentricLerp(v0.normal, v1.normal, v2.normal, barycentrics));
    // vtx.UV = BarycentricLerp(v0.UV, v1.UV, v2.UV, barycentrics);
    // vtx.Tangent = normalize(BarycentricLerp(v0.Tangent, v1.Tangent, v2.Tangent, barycentrics));
    // vtx.Bitangent = normalize(BarycentricLerp(v0.Bitangent, v1.Bitangent, v2.Bitangent, barycentrics));

    return vtx;
}

/* Random Functions */
uint xorShift32(inout uint state) {
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}

float randomFloat(inout uint state) {
    return (float)xorShift32(state) / 4294967296.0f;
}

float randomFloat(float min, float max, inout uint state) {
    return min + (max - min) * randomFloat(state);
}

float3 randomFloat3(inout uint state) {
    return float3(
        randomFloat(state),
        randomFloat(state),
        randomFloat(state)
    );
}

float3 randomFloat3(float min, float max, inout uint state) {
    return float3(
        randomFloat(min, max, state),
        randomFloat(min, max, state),
        randomFloat(min, max, state)
    );
}

float3 randomUnitSphereFloat3(inout uint state) {
    while (true) {
        const float3 u = randomFloat3(-1.0f, 1.0f, state);

        if (dot(u, u) < 1.0f) {
            return u;
        }
    }
}

float3 randomUnitFloat3(inout uint state) {
    return normalize(randomUnitSphereFloat3(state));
}

float3 randomHemisphereFloat3(float3 normal, inout uint state) {
    const float3 onUnitSphere = randomUnitFloat3(state);

    if (dot(onUnitSphere, normal) > 0.0f) {
        return onUnitSphere;
    }

    return -onUnitSphere;
}

#endif