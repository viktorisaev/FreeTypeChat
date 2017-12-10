#include "pch.h"

#include "ScreenGrid.h"
#include "..\Common\DirectXHelper.h"


namespace FreeTypeChat
{

using namespace DirectX;


ScreenGrid::ScreenGrid()
{
}


ScreenGrid::~ScreenGrid()
{
}


// loading resources requred async operations: load shaders from files of disk with "DX::ReadDataAsync". The rest could be done synchroneously
Concurrency::task<void> ScreenGrid::LoadGridResources(std::shared_ptr<DX::DeviceResources> _DeviceResources)
{
	// Create a root signature with a single constant buffer slot.
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(0, nullptr, 0, nullptr, rootSignatureFlags);

		Microsoft::WRL::ComPtr<ID3DBlob> pSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		DX::ThrowIfFailed(_DeviceResources->GetD3DDevice()->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		NAME_D3D12_OBJECT(m_rootSignature);
	}


	// Load shaders asynchronously.
	auto createVSTask = DX::ReadDataAsync(L"GridVertexShader.cso").then([this](std::vector<byte>& fileData) {
		m_vertexShader = fileData;
	});

	auto createPSTask = DX::ReadDataAsync(L"GridPixelShader.cso").then([this](std::vector<byte>& fileData) {
		m_pixelShader = fileData;
	});


	// Create the pipeline state once the shaders are loaded.
	auto createPipelineStateTask = (createPSTask && createVSTask).then([this, _DeviceResources]() {

		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = m_rootSignature.Get();
		state.VS = CD3DX12_SHADER_BYTECODE(&m_vertexShader[0], m_vertexShader.size());
		state.PS = CD3DX12_SHADER_BYTECODE(&m_pixelShader[0], m_pixelShader.size());
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		state.SampleMask = UINT_MAX;
		//		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = _DeviceResources->GetBackBufferFormat();
		state.DSVFormat = _DeviceResources->GetDepthBufferFormat();
		state.SampleDesc.Count = 1;

		state.BlendState.AlphaToCoverageEnable = false;
		state.BlendState.IndependentBlendEnable = false;
		state.BlendState.RenderTarget[0].BlendEnable = true;
		state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		state.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		state.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		state.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		state.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		state.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		DX::ThrowIfFailed(_DeviceResources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_pipelineState)));

		// Shader data can be deleted once the pipeline state is created.
		m_vertexShader.clear();
		m_pixelShader.clear();
	});

	return createPipelineStateTask;
}



// synchroneous creation of resources (including CommandList operations)
void ScreenGrid::InitializeGrid(const std::shared_ptr<DX::DeviceResources>& _DeviceResources, ID3D12GraphicsCommandList *_ResourceCreationCommandList, const int _NumOfVert, const int _NumOfHor)
{
	m_NumOfVert = _NumOfVert;
	m_NumOfHor = _NumOfHor;

	// Create and upload cube geometry resources to the GPU.
	auto d3dDevice = _DeviceResources->GetD3DDevice();

	// Grid.
	m_GridVertices = new VertexPosition[(m_NumOfVert + m_NumOfHor) * 2];

	VertexPosition *pos = m_GridVertices;

	for (int i = 0, ei = m_NumOfVert; i < ei; ++i)
	{
		float x = -1.0f + (2.0f * (i + 1)) / (m_NumOfVert + 1);
		*pos++ = { XMFLOAT3(x, -1.0f, 0.0f) };
		*pos++ = { XMFLOAT3(x,  1.0f, 0.0f) };
	}

	for (int i = 0, ei = m_NumOfHor; i < ei; ++i)
	{
		float y = -1.0f + (2.0f * (i + 1)) / (m_NumOfHor + 1);
		*pos++ = { XMFLOAT3(-1.0f, y, 0.0f) };
		*pos++ = { XMFLOAT3(1.0f, y, 0.0f) };
	}

	const UINT gridVerticesBufferSize = (m_NumOfVert + m_NumOfHor) * 2 * sizeof(VertexPosition);

	// Create the vertex buffer resource in the GPU's default heap and copy vertex data into it using the upload heap.
	// The upload resource must not be released until after the GPU has finished using it.

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC gridVertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(gridVerticesBufferSize);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &gridVertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_GridVertexBuffer)));
	NAME_D3D12_OBJECT(m_GridVertexBuffer);

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &gridVertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_GridVertexBufferUpload)));	// release by ComPtr
	NAME_D3D12_OBJECT(m_GridVertexBufferUpload);

	// Upload the vertex buffer to the GPU.
	D3D12_SUBRESOURCE_DATA m_GridVertexData = {};
	m_GridVertexData.pData = reinterpret_cast<BYTE*>(m_GridVertices);
	m_GridVertexData.RowPitch = gridVerticesBufferSize;
	m_GridVertexData.SlicePitch = m_GridVertexData.RowPitch;

	UpdateSubresources(_ResourceCreationCommandList, m_GridVertexBuffer.Get(), m_GridVertexBufferUpload.Get(), 0, 0, 1, &m_GridVertexData);

	_ResourceCreationCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GridVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	m_GridVertexBufferView.BufferLocation = m_GridVertexBuffer->GetGPUVirtualAddress();
	m_GridVertexBufferView.StrideInBytes = sizeof(VertexPosition);
	m_GridVertexBufferView.SizeInBytes = gridVerticesBufferSize;
}


void ScreenGrid::Render(ID3D12GraphicsCommandList * _CommandList)
{
	_CommandList->SetPipelineState(m_pipelineState.Get());

	_CommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	_CommandList->IASetVertexBuffers(0, 1, &m_GridVertexBufferView);
	_CommandList->DrawInstanced((m_NumOfVert + m_NumOfHor) * 2, 1, 0, 0);
}


}	// of namespace FreeTypeChat