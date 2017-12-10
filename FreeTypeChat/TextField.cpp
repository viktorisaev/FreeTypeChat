#include "pch.h"

#include "TextField.h"
#include "..\Common\DirectXHelper.h"


namespace FreeTypeChat
{

using namespace DirectX;


TextField::TextField() :
	m_CurrentNumberOfChars(0)
{
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



void TextField::AddCharacter(Character _Char)
{
	m_TextfieldRectangles[m_CurrentNumberOfChars] = _Char;
	m_CurrentNumberOfChars += 1;

	MapToVertices();
}






void TextField::MapToVertices()
{
	VertexPositionTexture *vertices = m_TextfieldVertices;

	for (int i = 0, ei = m_CurrentNumberOfChars; i < ei; ++i)
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








void TextField::Render(ID3D12GraphicsCommandList * _CommandList)
{
//	_CommandList->SetPipelineState(m_pipelineState.Get());

//	_CommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);		// TODO: use triangle list to simplify as strip doesn't work for separate characters (rects). Possible solution: geometry shader.
	_CommandList->IASetVertexBuffers(0, 1, &m_TextfieldVertexBufferView);
	_CommandList->DrawInstanced(m_CurrentNumberOfChars * 6, 1, 0, 0);
}











}	// of namespace FreeTypeChat