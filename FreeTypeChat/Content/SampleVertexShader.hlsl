#include "Common.hlsli"


VertexToPixelTextured main(InputToVertexTextured input)
{
	VertexToPixelTextured output;
	float4 pos = float4(input.pos, 1.0f);

	output.pos = pos;

	output.uv = input.uv;

	return output;
}
