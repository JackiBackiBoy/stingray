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
    uint gBufferPositionIndex;
    uint gBufferAlbedoIndex;
    uint gBufferNormalIndex;
    uint aoIndex;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

/* Samplers */
SamplerState g_LinearSampler : register(s0);

static const float AMBIENT_TERM = 0.3f;
static const float3 DIR_TO_SUN = float3(1.0f, 3.0f, -2.0f);

PSOutput main(PSInput input) {
    Texture2D<float4> positionTexture = ResourceDescriptorHeap[pushConstant.gBufferPositionIndex];
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[pushConstant.gBufferAlbedoIndex];
    Texture2D<float4> normalTexture = ResourceDescriptorHeap[pushConstant.gBufferNormalIndex];
    Texture2D<float4> aoTexture = ResourceDescriptorHeap[pushConstant.aoIndex];

    float3 fragPos = positionTexture.Sample(g_LinearSampler, input.texCoord).rgb;
    float3 albedo = albedoTexture.Sample(g_LinearSampler, input.texCoord).rgb;
    float3 normal = normalize(normalTexture.Sample(g_LinearSampler, input.texCoord).rgb);
    float ambientTerm = AMBIENT_TERM * aoTexture.Sample(g_LinearSampler, input.texCoord).r;

    // Diffuse lighting
    // TODO: This might have to be further investigated for HDR output
    float3 normDirToSun = normalize(DIR_TO_SUN);
    float diffuse = max(dot(normal, normDirToSun), 0.0);

    // Specular highlight (blinn-phong)
    float3 fragToViewDir = normalize(g_PerFrameData.cameraPosition - fragPos);
    float3 halfwayDir = normalize(normDirToSun + fragToViewDir);
    float specular = diffuse * pow(max(dot(normal, halfwayDir), 0.0f), 16.0f);

    PSOutput output;
    output.color = float4((ambientTerm + diffuse + specular) * albedo, 1);

    return output;
}