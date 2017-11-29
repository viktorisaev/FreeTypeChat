#include "Common.hlsli"

Texture2D texDiffuse : register(t0);
SamplerState textureSampler : register(s0);


// textured pixel

float4 main(VertexToPixelTextured input) : SV_TARGET
{
	float4 diffuse = texDiffuse.Sample(textureSampler, input.uv);

	return diffuse;
}
