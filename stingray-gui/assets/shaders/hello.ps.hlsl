struct PSInput {
    float4 position : SV_POSITION;
};

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(PSInput input) {
    PSOutput output = (PSOutput)0;
    output.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    
    return output;
}