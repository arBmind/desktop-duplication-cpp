struct VertexInput {
    float4 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

VertexOutput main(VertexInput input) {
    return input;
}
