#pragma once

namespace FreeTypeChat
{
	//// Constant buffer used to send MVP matrices to the vertex shader.
	//struct ModelViewProjectionConstantBuffer
	//{
	//	DirectX::XMFLOAT4X4 model;
	//	DirectX::XMFLOAT4X4 view;
	//	DirectX::XMFLOAT4X4 projection;
	//};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionTexture
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};

	struct VertexPosition
	{
		DirectX::XMFLOAT3 pos;
	};



	// not shader structure :)
	class Rectangle
	{
	public:
		Rectangle() :
			m_Pos(0.0f, 0.0f)
			, m_Size(0.0f, 0.0f)
		{ }
		Rectangle(DirectX::XMFLOAT2 _Pos, DirectX::XMFLOAT2 _Size) :
			m_Pos(_Pos)
			, m_Size(_Size)
		{ }


	public:
		DirectX::XMFLOAT2 m_Pos;
		DirectX::XMFLOAT2 m_Size;
	};

}