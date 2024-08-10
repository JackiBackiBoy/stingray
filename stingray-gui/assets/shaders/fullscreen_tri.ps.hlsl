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

struct DirectionLight {
    float4x4 lightSpaceMatrix;
    float4 color;
    float3 direction;
};

struct LightingUBO {
    DirectionLight directionLight;
    uint pad1;
};

struct PushConstant {
    uint gBufferPositionIndex;
    uint gBufferAlbedoIndex;
    uint gBufferNormalIndex;
    uint depthIndex;
    uint shadowMapIndex;
    uint aoIndex;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<LightingUBO> g_LightingUBO : register(b0, space1);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

/* Samplers */
SamplerState g_LinearSampler : register(s0);
SamplerState g_DepthSampler : register(s1);

static const float AMBIENT_TERM = 0.3f;

float calcShadow(DirectionLight light, float3 fragPos, float3 normal) {
    Texture2D<float4> depthTexture = ResourceDescriptorHeap[pushConstant.depthIndex];
    Texture2D<float4> shadowTexture = ResourceDescriptorHeap[pushConstant.shadowMapIndex];

    float4 fragPosLS = mul(light.lightSpaceMatrix, float4(fragPos, 1.0f));
    float3 projCoords = fragPosLS.xyz / fragPosLS.w; // perspective division
    projCoords = float3(projCoords.xy * 0.5f + 0.5f, projCoords.z);
    projCoords.y = 1.0f - projCoords.y; // TODO: might not be needed

    float shadowDepth = shadowTexture.Sample(g_DepthSampler, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float diffuseFactor = dot(normal, normalize(light.direction));
    float bias = lerp(0.005f, 0.0001f, diffuseFactor);

    float shadow = shadowDepth + bias < currentDepth ? 0.0f : 1.0f;

    return shadow;
}

PSOutput main(PSInput input) {
    Texture2D<float4> positionTexture = ResourceDescriptorHeap[pushConstant.gBufferPositionIndex];
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[pushConstant.gBufferAlbedoIndex];
    Texture2D<float4> normalTexture = ResourceDescriptorHeap[pushConstant.gBufferNormalIndex];
    Texture2D<float4> aoTexture = ResourceDescriptorHeap[pushConstant.aoIndex];

    DirectionLight dirLight = g_LightingUBO.directionLight;

    float3 fragPos = positionTexture.Sample(g_LinearSampler, input.texCoord).rgb;
    float3 albedo = albedoTexture.Sample(g_LinearSampler, input.texCoord).rgb;
    float3 normal = normalize(normalTexture.Sample(g_LinearSampler, input.texCoord).rgb);
    float ambientTerm = AMBIENT_TERM * aoTexture.Sample(g_LinearSampler, input.texCoord).r;

    // Shadows
    float shadow = calcShadow(dirLight, fragPos, normal);

    // Diffuse lighting
    // TODO: This might have to be further investigated for HDR output
    float3 normDirLight = normalize(dirLight.direction);
    float diffuse = max(dot(normal, normDirLight), 0.0);

    // Specular highlight (blinn-phong)
    float3 fragToViewDir = normalize(g_PerFrameData.cameraPosition - fragPos);
    float3 halfwayDir = normalize(normDirLight + fragToViewDir);
    float specular = diffuse * pow(max(dot(normal, halfwayDir), 0.0f), 16.0f);

    PSOutput output;
    output.color = float4((ambientTerm + (shadow) * (diffuse + specular)) * albedo, 1);

    return output;
}