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
uint initRand(in uint v0, in uint v1, in uint backoff = 16) {
    uint s0 = 0;

    for (uint n = 0; n < backoff; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

float nextRand(inout uint seed) {
	seed = (1664525u * seed + 1013904223u);
	return float(seed & 0x00FFFFFF) / float(0x01000000);
}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
	float3 a = abs(u);
	uint xm = ((a.x - a.y)<0 && (a.x - a.z)<0) ? 1 : 0;
	uint ym = (a.y - a.z)<0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

float3 getRandCosHemisphereSample(in float3 normal, inout uint seed) {
    // Get 2 random numbers to select our sample with
	float2 randVal = float2(nextRand(seed), nextRand(seed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = getPerpendicularVector(normal);
	float3 tangent = cross(bitangent, normal);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal.xyz * sqrt(1 - randVal.x);
}

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