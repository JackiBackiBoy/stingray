struct PerFrameData {
    float4x4 projection;
    float3 cameraPosition;
    float pad1;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0, space0);
ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

//static const float3 camera = float3(0.5, 2, -2);

[shader("raygeneration")]
void MyRaygenShader()
{
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

    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    // Write the raytraced color to the output texture.
    RenderTarget[idx] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(1, 0, 0, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}