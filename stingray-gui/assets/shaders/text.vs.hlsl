struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
    uint texIndex : TEXINDEX;
};

struct TextParams {
    float4 color;
    float2 position;
    float2 size;
    float2 texCoords[4];
    uint texIndex;
    uint pad1;
    uint pad2;
    uint pad3;
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

StructuredBuffer<TextParams> g_TextParamsBuffer : register(t0, space101);
ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

static const float2 g_Vertices[4] = {
    float2(0.0f, 0.0f), // top left
    float2(1.0f, 0.0f), // top right
    float2(1.0f, 1.0f), // bottom right
    float2(0.0f, 1.0f) // bottom left
};

static const uint g_Indices[6] = {
    0, 1, 2, 2, 3, 0
};

VSOutput main(uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID) {
    TextParams textParam = g_TextParamsBuffer[InstanceID];
    uint vertexIndex = g_Indices[VertexID];
    float2 vertex = g_Vertices[vertexIndex];
    float2 position = float2(
        vertex.x * textParam.size.x + textParam.position.x,
        vertex.y * textParam.size.y + textParam.position.y
    );

    VSOutput output;
    output.position = mul(pushConstant.uiProjection, float4(position, 0.0, 1.0));
    output.color = textParam.color;
    output.texCoord = textParam.texCoords[vertexIndex];
    output.texIndex = textParam.texIndex;

    return output;
}