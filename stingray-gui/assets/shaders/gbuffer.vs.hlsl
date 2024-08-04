struct VSInput {
    float3 position: POSITION;
    float3 normal: NORMAL;
    float3 tangent: TANGENT;
    float2 texCoord: TEXCOORD0;
};

struct VSOutput {
    float4 position: SV_Position;
    float3 positionWorldSpace : POSITION;
    float3 normal: NORMAL;
    float2 texCoord: TEXCOORD0;
    float3x3 TBN : TBN_MATRIX;
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

VSOutput main(VSInput input) {
    float4 positionWorld = mul(pushConstant.modelMatrix, float4(input.position, 1.0));

    VSOutput output;
    output.position = mul(g_PerFrameData.viewMatrix, positionWorld);
    output.position = mul(g_PerFrameData.projectionMatrix, output.position);
    output.positionWorldSpace = positionWorld.xyz;
    output.normal = normalize(input.normal);

    // Tangents
    float3 tangent = normalize(input.tangent);

    float3 T = normalize(float3(mul(pushConstant.modelMatrix, float4(tangent, 0.0f)).xyz));
    float3 N = normalize(float3(mul(pushConstant.modelMatrix, float4(output.normal, 0.0f)).xyz));
    float3 B = cross(N, T);
    output.TBN = transpose(float3x3(T, B, N));

    output.texCoord = input.texCoord;
    
    return output;
}