struct VertexInput {
    float2 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct VertexOutput {
    float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	float2 bgCoord : TEXCOORD1;
};

float2 posToCoord(float2 pos) {
	return float2(
		(1 + pos.x) / 2,
		(1 - pos.y) / 2
		);
}

VertexOutput main(VertexInput input) {
	VertexOutput ret;
	ret.position = float4(input.position, 0, 1);
	ret.texCoord = input.texCoord;
	ret.bgCoord = posToCoord(input.position);
	return ret;
}
