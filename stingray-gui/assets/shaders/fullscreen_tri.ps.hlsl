#define MAX_POINT_LIGHTS 32
#define MAX_CASCADES 8

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
    float4x4 cascadeProjections[MAX_CASCADES];
    float4x4 viewMatrix;
    float4 color;
    float4 cascadeDistances[2];
    float3 direction;
    uint numCascades;
};

struct PointLight {
    float4 color; // NOTE: w-component is intensity
    float3 position;
    uint pad1;
};

struct LightingUBO {
    DirectionLight directionLight;
    uint numPointLights;
    uint pad1;
    uint pad2;
    uint pad3;
    PointLight pointLights[MAX_POINT_LIGHTS];
};

struct PushConstant {
    uint gBufferPositionIndex;
    uint gBufferAlbedoIndex;
    uint gBufferNormalIndex;
    uint depthIndex;
    uint shadowMapIndex;
    uint aoIndex;
    float shadowMinBias;
    float shadowMaxBias;
};

ConstantBuffer<PerFrameData> g_PerFrameData : register(b0, space0);
ConstantBuffer<LightingUBO> g_LightingUBO : register(b0, space1);
ConstantBuffer<PushConstant> pushConstant : register(b0, space2);

/* Samplers */
SamplerState g_LinearSampler : register(s0);
SamplerComparisonState g_DepthSampler : register(s1);

static const float AMBIENT_TERM = 0.3f;

float calcShadow(DirectionLight light, float3 fragPos, float3 normal) {
    float4 fragPosLightView = mul(g_PerFrameData.viewMatrix, float4(fragPos, 1.0f));
    float depthValue = abs(fragPosLightView.z);

    int cascadeLayer = -1;
    float2 texCoordOrigin = float2(0, 0);
    float cascadeDistances[MAX_CASCADES] = (float[MAX_CASCADES])light.cascadeDistances;

    for (int i = 0; i < light.numCascades; ++i) {
        if (depthValue < cascadeDistances[i]) {
            cascadeLayer = i;
            texCoordOrigin = float2(0.5f * (i % 2), 0.5f * (i / 2));
            break;
        }
    }

    float4 fragPosLS = mul(light.cascadeProjections[cascadeLayer], float4(fragPos, 1.0f));
    float3 projCoords = fragPosLS.xyz / fragPosLS.w; // perspective division
    projCoords = float3(projCoords.xy * 0.5f + 0.5f, projCoords.z);
    projCoords.y = 1.0f - projCoords.y;

    float diffuseFactor = dot(normal, normalize(light.direction));
    float bias = lerp(pushConstant.shadowMaxBias, pushConstant.shadowMinBias, diffuseFactor);

    float currentDepth = projCoords.z;

    Texture2D<float4> shadowTexture = ResourceDescriptorHeap[pushConstant.shadowMapIndex];
    float shadow = 0.0f;
    float texelSize = 1.0f / 4096;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            shadow += shadowTexture.SampleCmpLevelZero(g_DepthSampler, texCoordOrigin + projCoords.xy * 0.5f + float2(x, y) * texelSize, currentDepth - bias).r;
        }
    }

    shadow /= 9.0f;

    return shadow;
}

float3 calcDirectionLight(DirectionLight light, float3 fragPos, float3 fragNormal) {
    float3 normLightDir = normalize(light.direction);

    // Diffuse factor
    float diffuse = max(dot(fragNormal, normLightDir), 0.0);

    // Specular highlight (blinn-phong)
    float3 fragToViewDir = normalize(g_PerFrameData.cameraPosition - fragPos);
    float3 halfwayDir = normalize(normLightDir + fragToViewDir);
    float specular = diffuse * pow(max(dot(fragNormal, halfwayDir), 0.0f), 16.0f);

    return (diffuse + specular) * light.color.xyz;
}

float3 calcPointLight(PointLight light, float3 fragPos, float3 fragNormal) {
    float3 dirToLight = light.position - fragPos;
    float3 normDirToLight = normalize(light.position - fragPos);

    // Diffuse factor
    float diffuse = max(dot(fragNormal, normDirToLight), 0.0);

    // Specular highlight (blinn-phong)
    float3 fragToViewDir = normalize(g_PerFrameData.cameraPosition - fragPos);
    float3 halfwayDir = normalize(normDirToLight + fragToViewDir);
    float specular = diffuse * pow(max(dot(fragNormal, halfwayDir), 0.0f), 16.0f);

    // Attenuation (inverse square law)
    float attenuation = 1.0f / dot(dirToLight, dirToLight);
    float3 intensity = light.color.w * attenuation;

    return (diffuse + specular) * intensity * light.color.xyz;
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

    // Direction light contribution
    float3 dirLightContrib = calcDirectionLight(dirLight, fragPos, normal);
    float3 pointLightContrib = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < g_LightingUBO.numPointLights; ++i) {
        pointLightContrib += calcPointLight(g_LightingUBO.pointLights[i], fragPos, normal);
    }

    PSOutput output;
    output.color = float4((ambientTerm + (shadow) * (dirLightContrib + pointLightContrib)) * albedo, 1);

    return output;
}