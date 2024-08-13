#ifndef GLOBALS_HEADER
#define GLOBALS_HEADER

struct PerFrameData {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 invViewProjection;
    float3 cameraPosition;
    uint pad1;
};

#endif