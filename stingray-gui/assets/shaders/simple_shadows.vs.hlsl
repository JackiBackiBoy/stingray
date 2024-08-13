#include "globals.hlsl"

struct VSInput {
    float3 position: POSITION;
    float3 normal: NORMAL;
    float3 tangent: TANGENT;
    float2 texCoord: TEXCOORD0;
};

struct VSOutput {
    float4 position: SV_Position;
};

struct ShadowUBO {
    float4x4 lightSpaceMatrix;
};

struct PushConstant {
    float4x4 modelMatrix;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<ShadowUBO> g_ShadowUBO : register(b0, space1);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

VSOutput main(VSInput input) {
    float4 positionWorld = mul(pushConstant.modelMatrix, float4(input.position, 1.0));

    VSOutput output;
    output.position = mul(g_ShadowUBO.lightSpaceMatrix, positionWorld);
    
    return output;
}