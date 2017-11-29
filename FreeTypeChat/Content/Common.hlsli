struct InputToVertexPosition
{
	float3 pos : POSITION;
};

struct InputToVertexTextured
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertexToPixelPosition
{
	float4 pos : SV_POSITION;
};


struct VertexToPixelTextured
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};