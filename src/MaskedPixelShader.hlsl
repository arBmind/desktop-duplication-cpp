Texture2D bgTexture : register(t0);
Texture2D maskedTexture : register(t1);
SamplerState bgSampler : register(s0);
SamplerState maskedSampler : register(s1);

struct Input {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	float2 bgCoord : TEXCOORD1;
};

// shadermodel 4 has no bit operations
// normalized float [0..1] xor emulation for 8 bits
float4 fnxor8(float4 l, float4 r) {
	float4 o = float4(0, 0, 0, 1);
	float4 i = float4(1, 1, 1, 0);
	for (int c = 0; c < 8; c++) {
		i /= 2;
		float4 bl = step(i, l);
		float4 br = step(i, r);
		l -= bl * i;
		r -= br * i;
		o += abs(bl - br) * i;
	}
	return o;
}

float4 main(Input input) : SV_TARGET{
	float4 mc = maskedTexture.Sample(maskedSampler, input.texCoord);
	if (mc.a != 0) {
		float4 bg = bgTexture.Sample(bgSampler, input.bgCoord);
		return fnxor8(mc, bg);
	}
	return float4(mc.rgb, 1);
}
