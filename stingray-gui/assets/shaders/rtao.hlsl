#ifndef RTAO_HLSL
#define RTAO_HLSL

#include "raytracing_math.hlsl"

struct PerFrameData {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewProjection;
    float3 cameraPosition;
    uint pad1;
};

struct PushConstant {
    uint gBufferPositionIndex;
    uint gBufferNormalIndex;
    uint frameCount;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0, space0);
ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
StructuredBuffer<GeometryInfo> g_GeometryInfo : register(t1, space0);
StructuredBuffer<MaterialInfo> g_MaterialInfo : register(t2, space0);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload {
    float4 color;
    uint randSeed;
    uint depth;
};

static const float3 g_SkyColor = float3(0.623, 0.8, 0.913);
static const float3 g_SunDirection = normalize(float3(0.4, 1.0, -0.5)); // NOTE: Direction TO sun from origin

uint initRand(in uint v0, in uint v1, in uint backoff = 16) {
    uint s0 = 0;

    for (uint n = 0; n < backoff; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

float shootAORay(in float3 origin, in float3 dir, in float minT, in float maxT) {
    RayPayload payload;
    payload.color = float4(0, 0, 0, 1);

    RayDesc aoRay;
    aoRay.Origin = origin;
    aoRay.Direction = dir;
    aoRay.TMin = minT;
    aoRay.TMax = maxT;

    TraceRay(Scene, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 1, 0, aoRay, payload);

    return payload.color.x;
}

[shader("raygeneration")]
void MyRaygenShader() {
    uint2 idx = DispatchRaysIndex().xy;
    uint2 numPixels = DispatchRaysDimensions().xy;
    uint randSeed = initRand(idx.x + idx.y * numPixels.x + 1, pushConstant.frameCount);

    Texture2D<float4> gBufferPositionTexture = ResourceDescriptorHeap[pushConstant.gBufferPositionIndex];
    Texture2D<float4> gBufferNormalTexture = ResourceDescriptorHeap[pushConstant.gBufferNormalIndex];

    float4 worldPos = gBufferPositionTexture[idx];
    float3 worldNormal = gBufferNormalTexture[idx].xyz;

    float aoValue = 1.0f;
    float minT = 0.001f;
    float maxT = 10.0f;

    if (worldPos.w != 0.0f) {
        float3 worldDir = randomHemisphereFloat3(worldNormal, randSeed);
        aoValue = shootAORay(worldPos.xyz, worldDir, minT, maxT);
    }
    
    RenderTarget[idx] = float4(aoValue, aoValue, aoValue, 1.0f);
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr) {
    payload.color = float4(1, 1, 1, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload) {
    //payload.color = float4(g_SkyColor, 1);
    payload.color = float4(1, 1, 1, 1);
}

#endif