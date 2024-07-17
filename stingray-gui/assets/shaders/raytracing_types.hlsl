#ifndef RAYTRACING_TYPES_HLSL
#define RAYTRACING_TYPES_HLSL

struct GeometryInfo { // NOTE: Per BLAS
	uint vertexBufferIndex;
	uint indexBufferIndex;
};

struct MaterialInfo {
    float4 color;
    float roughness;
};

struct Vertex {
    float3 position;
    float3 normal;
    float3 tangent;
    float2 texCoord;
};

#endif