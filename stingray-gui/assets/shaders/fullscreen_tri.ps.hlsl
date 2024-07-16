struct PSInput {
    float4 pos: SV_Position;
    float2 texCoord: TEXCOORD0;
};

struct PSOutput {
    float4 color: SV_Target0;
};

struct PushConstant {
    uint renderTargetIndex;
};

ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

/* Samplers */
SamplerState g_LinearSampler : register(s0);

PSOutput main(PSInput input) {
    PSOutput output;
    Texture2D<float4> renderTarget = ResourceDescriptorHeap[pushConstant.renderTargetIndex];
    output.color = float4(float4(renderTarget.Sample(g_LinearSampler, input.texCoord)).rgb, 1);

    return output;
}