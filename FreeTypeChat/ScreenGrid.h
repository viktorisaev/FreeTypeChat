// render screen space grid

#pragma once

#include <ppltasks.h>

#include "..\Common\DeviceResources.h"
#include "Content/ShaderStructures.h"


namespace FreeTypeChat
{

class ScreenGrid
{
public:
	ScreenGrid();
	~ScreenGrid();

	Concurrency::task<void> LoadGridResources(std::shared_ptr<DX::DeviceResources> _DeviceResources);
	void InitializeGrid(std::shared_ptr<DX::DeviceResources> _DeviceResources, ID3D12GraphicsCommandList *_ResourceCreationCommandList, const int _NumOfVert, const int _NumOfHor);

	void Render(ID3D12GraphicsCommandList *_CommandList);

private:

	int m_NumOfVert;
	int m_NumOfHor;

	Microsoft::WRL::ComPtr<ID3D12Resource>				m_GridVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW							m_GridVertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;

	D3D12_RECT											m_scissorRect;
	std::vector<byte>									m_vertexShader;
	std::vector<byte>									m_pixelShader;

	Concurrency::task<void> m_LoaderTask;
	VertexPosition *m_GridVertices;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_GridVertexBufferUpload;

};


}
