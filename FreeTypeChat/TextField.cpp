#include "pch.h"

#include "TextField.h"
#include "..\Common\DirectXHelper.h"


namespace FreeTypeChat
{

using namespace DirectX;


TextField::TextField() :
	m_TextfieldRectangles()
{
	m_BeginCaretPos = DirectX::XMFLOAT2(m_LeftTextfieldSide, 0.95f);

	m_TextfieldRectangles.reserve(N_CHARS);
}


TextField::~TextField()
{
}




void TextField::InitializeTextfield(const std::shared_ptr<DX::DeviceResources>& _DeviceResources)
{

	// Create and upload cube geometry resources to the GPU.
	auto d3dDevice = _DeviceResources->GetD3DDevice();

	// Vertices.

	const UINT textfieldVerticesBufferSize = N_CHARS * 6 * sizeof(VertexPositionTexture);

	// Create the vertex buffer resource in the GPU's update heap. Initialized/Updated in "Update" method.
	CD3DX12_RESOURCE_DESC textfieldVertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(textfieldVerticesBufferSize);
	// ResourceState is "D3D12_RESOURCE_STATE_GENERIC_READ" only for "UPLOAD" heap type
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &textfieldVertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_TextfieldVertexBuffer)));
	NAME_D3D12_OBJECT(m_TextfieldVertexBuffer);

	// map textfield vertex buffer (position)
	CD3DX12_RANGE readRange(0, 0);
	m_TextfieldVertexBuffer->Map(0, &readRange, &((void*)m_TextfieldVertices));

	m_TextfieldVertexBufferView.BufferLocation = m_TextfieldVertexBuffer->GetGPUVirtualAddress();
	m_TextfieldVertexBufferView.StrideInBytes = sizeof(VertexPositionTexture);
	m_TextfieldVertexBufferView.SizeInBytes = textfieldVerticesBufferSize;

//	m_texDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}






void TextField::Update()
{
}



void TextField::AddCharacter(UINT _Pos, Character _Char)
{
	//DirectX::XMFLOAT2 curPos = GetCaretPosByIndex(_Pos);

	//if (curPos.x + (m_IntercharacterSpace + _Char.m_Geom.m_Size.x) > 0)	// text field width reached, move to the next line
	//{
	//	curPos.x = m_LeftTextfieldSide;
	//	curPos.y -= m_CharacterRowHeight;
	//}

	//_Char.m_Geom.m_Pos = curPos;	// HACK: fix position keeping size and texture "as is"

	m_TextfieldRectangles.insert(m_TextfieldRectangles.begin() +_Pos, _Char);

	RePositionCharacters(_Pos);

	MapToVertices();
}




void TextField::RePositionCharacters(UINT _StartPos)
{
	DirectX::XMFLOAT2 nextPos = m_BeginCaretPos;	// sart from top left

	for (UINT i = 0, ei = GetNumberOfChars(); i < ei; ++i)
	{
		DirectX::XMFLOAT2 pos = nextPos;

		Character curChar = m_TextfieldRectangles[i];
		float w = curChar.m_Geom.m_Size.x;
		float h = curChar.m_Geom.m_Size.y;

		nextPos.x += (m_IntercharacterSpace + w);

		if (nextPos.x > 0)	// text field width reached, move to the next line
		{
			nextPos.x = m_LeftTextfieldSide;
			nextPos.y -= m_CharacterRowHeight;

			pos = nextPos;

			nextPos.x += (m_IntercharacterSpace + w);
		}

		m_TextfieldRectangles[i].m_Geom.m_Pos = pos;

	}
}






void TextField::MapToVertices()
{
	VertexPositionTexture *vertices = m_TextfieldVertices;

	for (int i = 0, ei = GetNumberOfChars(); i < ei; ++i)
	{
		Character &rect = m_TextfieldRectangles[i];

		float dx = rect.m_Geom.m_Pos.x + rect.m_Geom.m_Size.x;
		float dy = rect.m_Geom.m_Pos.y - rect.m_Geom.m_Size.y;

		*vertices = { DirectX::XMFLOAT3(rect.m_Geom.m_Pos.x, rect.m_Geom.m_Pos.y, 0.0f), DirectX::XMFLOAT2(rect.m_Texture.m_Pos.x, rect.m_Texture.m_Pos.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(dx, rect.m_Geom.m_Pos.y, 0.0f), DirectX::XMFLOAT2(rect.m_Texture.m_Pos.x + rect.m_Texture.m_Size.x, rect.m_Texture.m_Pos.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(rect.m_Geom.m_Pos.x, dy, 0.0f), DirectX::XMFLOAT2(rect.m_Texture.m_Pos.x, rect.m_Texture.m_Pos.y + rect.m_Texture.m_Size.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(rect.m_Geom.m_Pos.x, dy, 0.0f), DirectX::XMFLOAT2(rect.m_Texture.m_Pos.x, rect.m_Texture.m_Pos.y + rect.m_Texture.m_Size.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(dx, rect.m_Geom.m_Pos.y, 0.0f), DirectX::XMFLOAT2(rect.m_Texture.m_Pos.x + rect.m_Texture.m_Size.x, rect.m_Texture.m_Pos.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(dx, dy, 0.0f), DirectX::XMFLOAT2(rect.m_Texture.m_Pos.x + rect.m_Texture.m_Size.x, rect.m_Texture.m_Pos.y + rect.m_Texture.m_Size.y) };
		++vertices;
	}
}







DirectX::XMFLOAT2 TextField::GetCaretPosByIndex(UINT _pos)
{
	DirectX::XMFLOAT2 curPos = m_BeginCaretPos;	// start from top left corner

	for (UINT i = 0, ei = _pos; i < ei; ++i)
	{
		Character curChar = m_TextfieldRectangles[i];
		float w = curChar.m_Geom.m_Size.x;
		float h = curChar.m_Geom.m_Size.y;

		curPos.x += (m_IntercharacterSpace + w);

		if (curPos.x > 0)	// text field width reached, move to the next line
		{
			curPos.x = m_LeftTextfieldSide + (m_IntercharacterSpace + w);
			curPos.y -= m_CharacterRowHeight;
		}
	}

	// correct to next char (if not last)
	if (_pos < GetNumberOfChars())
	{
		Character curChar = m_TextfieldRectangles[_pos];
		float w = curChar.m_Geom.m_Size.x;
		float h = curChar.m_Geom.m_Size.y;

		if (curPos.x + (m_IntercharacterSpace + w) > 0)	// text field width reached, move to the next line
		{
			curPos.x = m_LeftTextfieldSide;
			curPos.y -= m_CharacterRowHeight;
		}
	}

	return curPos;
}








void TextField::Render(ID3D12GraphicsCommandList * _CommandList)
{
//	_CommandList->SetPipelineState(m_pipelineState.Get());

//	_CommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);		// TODO: use triangle list to simplify as strip doesn't work for separate characters (rects). Possible solution: geometry shader.
	_CommandList->IASetVertexBuffers(0, 1, &m_TextfieldVertexBufferView);
	_CommandList->DrawInstanced(GetNumberOfChars() * 6, 1, 0, 0);
}











}	// of namespace FreeTypeChat