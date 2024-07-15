#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "raytracing_math.hlsl"

struct PerFrameData {
    float4x4 projection;
    float3 cameraPosition;
    float pad1;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0, space0);
ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
StructuredBuffer<GeometryInfo> g_GeometryInfo : register(t1, space0);
StructuredBuffer<MaterialInfo> g_MaterialInfo : register(t2, space0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload {
    float4 color;
    uint randSeed;
    uint depth;
};

static const float3 g_SkyColor = float3(0.623, 0.8, 0.913);
static const float3 g_SunDirection = normalize(float3(0.4, 1.0, -0.5)); // NOTE: Direction TO sun from origin

[shader("raygeneration")]
void MyRaygenShader() {
    uint2 idx = DispatchRaysIndex().xy;

    float2 xy = idx + 0.5f; // center at the middle of pixel
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;
    screenPos.y = -screenPos.y;

    float4 world = mul(g_PerFrameData.projection, float4(screenPos, 0, 1));
    world.xyz /= world.w; // perspective divide w manually

    // Trace the ray
    RayDesc ray;
    ray.Origin = g_PerFrameData.cameraPosition;
    ray.Direction = normalize(world.xyz - g_PerFrameData.cameraPosition);
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.color = float4(0, 0, 0, 0);
    payload.randSeed = idx.x * idx.y + 1;
    payload.depth = 0;

    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture
    RenderTarget[idx] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr) {
    if (payload.depth == 1) {
        payload.color = float4(1, 1, 1, 1);
        return;
    }

    // TODO: Refactor this into multiple functions
    // TODO: Add logic for certain objects?
    // Hit world position
    float3 hitPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    // Get vertex and index buffers for the current BLAS
    GeometryInfo geoInfo = g_GeometryInfo[InstanceIndex()];
    MaterialInfo materialInfo = g_MaterialInfo[InstanceIndex()];
    StructuredBuffer<Vertex> vertexBuffer = ResourceDescriptorHeap[geoInfo.vertexBufferIndex];
    Buffer<uint> indexBuffer = ResourceDescriptorHeap[geoInfo.indexBufferIndex];

    const uint primIdx = PrimitiveIndex();
    const uint index0 = indexBuffer[primIdx * 3 + 0];
    const uint index1 = indexBuffer[primIdx * 3 + 1];
    const uint index2 = indexBuffer[primIdx * 3 + 2];

    const Vertex vertex0 = vertexBuffer[index0];
    const Vertex vertex1 = vertexBuffer[index1];
    const Vertex vertex2 = vertexBuffer[index2];

    // Barycentric lerp
    Vertex finalVertex = barycentricLerp(vertex0, vertex1, vertex2, barycentrics);
    
    // Diffuse lighting
    float3 wsNormal = normalize(mul(finalVertex.normal, (float3x3)ObjectToWorld4x3()));
    float diffuseTerm = max(dot(wsNormal, g_SunDirection), 0.0);

    if (payload.depth < 1) {
        float totalAO = 0.0f;
        int samplesPerPixel = 32;

        for (int i = 0; i < samplesPerPixel; ++i) {
            float3 randDir = randomHemisphereFloat3(wsNormal, payload.randSeed);

            RayDesc aoRay;
            aoRay.Origin = hitPosition;
            aoRay.Direction = randDir;
            aoRay.TMin = 0.001;
            aoRay.TMax = 10000.0;

            payload.color = float4(1, 1, 1, 1);
            payload.depth = payload.depth + 1;

            TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, aoRay, payload);
            totalAO += payload.color.x;
        }

        float avgAO = totalAO / float(samplesPerPixel);
        payload.color = float4(avgAO, avgAO, avgAO, 1);
    }

    // Mirror
    // if (materialInfo.roughness == 0.0f) {
    //     if (!payload.allowReflection) {
    //         return;
    //     }

    //     float3 worldNormal = normalize(mul(lerpedVertex.normal, (float3x3)ObjectToWorld4x3()));
    //     float3 reflectDir = reflect(normalize(WorldRayDirection()), worldNormal);

    //     RayDesc mirrorRay;
    //     mirrorRay.Origin = hitPosition;
    //     mirrorRay.Direction = reflectDir;
    //     mirrorRay.TMin = 0.001;
    //     mirrorRay.TMax = 10000.0;

    //     payload.allowReflection = false;
    //     TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, mirrorRay, payload);
    //     return;
    // }

    //payload.color = float4(diffuseTerm * materialInfo.color.xyz, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload) {
    //payload.color = float4(g_SkyColor, 1);
    payload.color = float4(0, 0, 0, 1);
}

#endif