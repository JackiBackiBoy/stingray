struct PSInput {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
    uint texIndex : TEXCOORD1;
};

struct PSOutput {
    float4 color : SV_Target0;
};

struct PerFrameData {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewProjection;
    float3 cameraPosition;
    uint pad1;
};

struct PushConstant {
    float4x4 uiProjection;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

SamplerState linearSampler : register(s0);

PSOutput main(PSInput input) {
    Texture2D texture = ResourceDescriptorHeap[input.texIndex];

    PSOutput output;
    output.color = input.color * float4(texture.Sample(linearSampler, input.texCoord));

    return output;
}