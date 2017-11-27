#include "Common.hlsli"

Texture2D texDiffuse : register(t0);
SamplerState textureSampler : register(s0);


float4 main(VertexToPixel input) : SV_TARGET
{
	float4 diffuse = texDiffuse.Sample(textureSampler, input.uv);

	return diffuse;
}
