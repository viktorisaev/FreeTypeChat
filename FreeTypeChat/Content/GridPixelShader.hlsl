#include "Common.hlsli"


// draw fixed color (grid)
float4 main(VertexToPixelPosition input) : SV_TARGET
{
	float4 diffuse = float4(0.3f, 0.5f, 0.8f, 1.0f);

	return diffuse;
}
