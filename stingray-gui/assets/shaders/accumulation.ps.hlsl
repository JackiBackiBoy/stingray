struct PSInput {
    float4 pos: SV_Position;
    float2 texCoord: TEXCOORD0;
};

struct PSOutput {
    float4 color: SV_Target0;
};

struct PerFrameData {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewProjection;
    float3 cameraPosition;
    uint pad1;
};

struct PushConstant {
    uint lastFrameIndex;
    uint currFrameIndex;
    uint accumulationCount;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

/* Samplers */
SamplerState g_LinearSampler : register(s0);

PSOutput main(PSInput input) {
    PSOutput output;
    Texture2D<float4> lastFrame = ResourceDescriptorHeap[pushConstant.lastFrameIndex];
    Texture2D<float4> currFrame = ResourceDescriptorHeap[pushConstant.currFrameIndex];

    const float4 lastColor = lastFrame.Sample(g_LinearSampler, input.texCoord);
    const float4 currColor = currFrame.Sample(g_LinearSampler, input.texCoord);

    // Weighted sum
    output.color = (pushConstant.accumulationCount * lastColor + currColor) / (pushConstant.accumulationCount + 1);
    return output;
}