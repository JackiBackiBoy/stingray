struct VSOutput {
    float4 pos: SV_Position;
    float2 texCoord: TEXCOORD0;
};

VSOutput main(uint VertexID : SV_VertexID) {
    // Calculate UV coordinates directly from the VertexID
    VSOutput output;
    output.texCoord.x = (VertexID << 1) & 2;
    output.texCoord.y = VertexID & 2;
    
    // Set position for a full-screen triangle
    output.pos = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    
    return output;
}