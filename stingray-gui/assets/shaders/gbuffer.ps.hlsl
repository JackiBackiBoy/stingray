struct PSInput {
    float4 position : SV_Position;
    float3 positionWorldSpace : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3x3 TBN : TBN_MATRIX;
};

struct PSOutput {
    float4 position : SV_Target0;
    float4 albedo : SV_Target1;
    float4 normal : SV_Target2;
};

struct PerFrameData {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewProjection;
    float3 cameraPosition;
    uint pad1;
};

struct PushConstant {
    float4x4 modelMatrix;
    uint albedoMapIndex;
    uint normalMapIndex;
    uint pad1;
    uint pad2;
    float3 color;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

SamplerState linearSampler : register(s0);

PSOutput main(PSInput input) {
    Texture2D albedoTexture = ResourceDescriptorHeap[pushConstant.albedoMapIndex];
    Texture2D normalMapTexture = ResourceDescriptorHeap[pushConstant.normalMapIndex];
    SamplerState sampler = linearSampler;

    PSOutput output;
    output.position = float4(input.positionWorldSpace, 1.0f);
    output.albedo = float4(pushConstant.color * albedoTexture.Sample(sampler, input.texCoord).xyz, 1.0f);

    // Normal
    float3 surfaceNormal = normalMapTexture.Sample(sampler, input.texCoord).xyz;
    surfaceNormal = normalize(surfaceNormal * 2.0f - 1.0f);
    surfaceNormal = normalize(float3(mul(input.TBN, surfaceNormal)));
    output.normal = float4(surfaceNormal, 1.0f);

    return output;
}