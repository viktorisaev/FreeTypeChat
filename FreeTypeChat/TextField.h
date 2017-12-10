// render screen space grid

#pragma once

#include <ppltasks.h>

#include "..\Common\DeviceResources.h"
#include "Content/ShaderStructures.h"


namespace FreeTypeChat
{


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


class Character
{
public:
	Rectangle m_Geom;
	Rectangle m_Texture;
};


class TextField
{
public:
	TextField();
	~TextField();

	void InitializeTextfield(const std::shared_ptr<DX::DeviceResources>& _DeviceResources);

	void Update();

	void Render(ID3D12GraphicsCommandList *_CommandList);

	void AddCharacter(Character _Char);

private:
	void MapToVertices();

private:

	const int N_CHARS = 1024;	// number of chars in the text field

	//Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
	//Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;

	Microsoft::WRL::ComPtr<ID3D12Resource>				m_TextfieldVertexBuffer;	// of D3D12_HEAP_TYPE_UPLOAD - to CPU write-only, GPU-read-only (or  multiply, depends on cache)
	D3D12_VERTEX_BUFFER_VIEW							m_TextfieldVertexBufferView;
	VertexPositionTexture								*m_TextfieldVertices;	// mapped vertices data
	Character											m_TextfieldRectangles[1024];


//	UINT												m_texDescriptorSize;

	int		m_CurrentNumberOfChars;

};


}
