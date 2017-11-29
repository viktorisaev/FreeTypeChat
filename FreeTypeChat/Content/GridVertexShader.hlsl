#include "Common.hlsli"


VertexToPixelPosition main(InputToVertexPosition input)
{
	VertexToPixelPosition output;
	float4 pos = float4(input.pos, 1.0f);

	output.pos = pos;

	return output;
}
