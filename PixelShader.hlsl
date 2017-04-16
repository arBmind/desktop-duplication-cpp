Texture2D texture0;
SamplerState sampler0;

struct Input {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(Input input) : SV_TARGET {
    return texture0.Sample(sampler0, input.texCoord);
}
