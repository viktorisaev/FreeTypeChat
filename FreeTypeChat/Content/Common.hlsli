struct InputToVertex
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertexToPixel
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};