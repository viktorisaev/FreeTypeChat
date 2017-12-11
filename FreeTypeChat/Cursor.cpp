#include "pch.h"

#include "Cursor.h"
#include "..\Common\DirectXHelper.h"


namespace FreeTypeChat
{

using namespace DirectX;


Cursor::Cursor()
{
}


Cursor::~Cursor()
{
}




void Cursor::InitializeCursorPipelineState(std::shared_ptr<DX::DeviceResources> _DeviceResources, const D3D12_SHADER_BYTECODE &_VertexShader, const D3D12_SHADER_BYTECODE &_PixelShader)
{
	{
		CD3DX12_DESCRIPTOR_RANGE range;
		CD3DX12_ROOT_PARAMETER parameter;

		// texture
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		parameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		//			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;	// pixel sdr has access


		// sampler for the texture
		D3D12_STATIC_SAMPLER_DESC samplerRing = {};
		samplerRing.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;	// no linear required
		samplerRing.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerRing.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerRing.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerRing.MipLODBias = 0;
		samplerRing.MaxAnisotropy = 0;
		samplerRing.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerRing.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerRing.MinLOD = 0.0f;
		samplerRing.MaxLOD = D3D12_FLOAT32_MAX;
		samplerRing.ShaderRegister = 0;															// SamplerState textureSampler : register(s0);
		samplerRing.RegisterSpace = 0;
		samplerRing.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(1, &parameter, 1, &samplerRing, rootSignatureFlags);

		Microsoft::WRL::ComPtr<ID3DBlob> pSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		DX::ThrowIfFailed(_DeviceResources->GetD3DDevice()->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		NAME_D3D12_OBJECT(m_rootSignature);
	}


	// Create the pipeline state once the shaders are loaded.
	{
		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = m_rootSignature.Get();
		state.VS = _VertexShader;
		state.PS = _PixelShader;
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;							// DON'T CARE ABOUT CULLING
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		state.DepthStencilState.DepthEnable = false;											// DISABLE DEPTH TEST!!!
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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
	}
}





void Cursor::InitializeCursor(const std::shared_ptr<DX::DeviceResources>& _DeviceResources, ID3D12DescriptorHeap* _texHeap, ID3D12GraphicsCommandList *_ResourceCreationCommandList, DirectX::XMFLOAT2	_CursorNormalizedSize, double _BlinkPeriod)
{
	m_CursorNormalizedSize = _CursorNormalizedSize;
	m_BlinkPeriod = _BlinkPeriod;


	// Create and upload cube geometry resources to the GPU.
	auto d3dDevice = _DeviceResources->GetD3DDevice();

	// Vertices.

	const UINT cursorVerticesBufferSize = 4 * sizeof(VertexPositionTexture);

	// Create the vertex buffer resource in the GPU's update heap. Initialized/Updated in "Update" method.
	CD3DX12_RESOURCE_DESC cursorVertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cursorVerticesBufferSize);
	// ResourceState is "D3D12_RESOURCE_STATE_GENERIC_READ" only for "UPLOAD" heap type
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &cursorVertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_CursorVertexBuffer)));
	NAME_D3D12_OBJECT(m_CursorVertexBuffer);

	// map cursor vertex buffer (position)
	CD3DX12_RANGE readRange(0, 0);
	m_CursorVertexBuffer->Map(0, &readRange, &((void*)m_CursorVertices));

	m_CursorVertexBufferView.BufferLocation = m_CursorVertexBuffer->GetGPUVirtualAddress();
	m_CursorVertexBufferView.StrideInBytes = sizeof(VertexPositionTexture);
	m_CursorVertexBufferView.SizeInBytes = cursorVerticesBufferSize;

	m_texDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// texture
	{
		D3D12_RESOURCE_DESC desc = {};

		desc.Width = static_cast<UINT>(4);
		desc.Height = static_cast<UINT>(4);		// texture of minimal size 4x4
		desc.MipLevels = static_cast<UINT16>(1);
		desc.DepthOrArraySize = static_cast<UINT16>(1);
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_CursorTexture)));
		NAME_D3D12_OBJECT(m_CursorTexture);
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_CursorTexture.Get(), 0, 1);

		// temp, should be deleted after used
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_UploadHeap.GetAddressOf())));
		NAME_D3D12_OBJECT(m_UploadHeap);

		D3D12_SUBRESOURCE_DATA subData = {};
		byte textur[4][4][4];

		for (int i = 0, ei = (int)desc.Height; i < ei; ++i)
		{
			for (int j = 0, ej = (int)desc.Width; j < ej; ++j)
			{
				textur[i][j][0] = 0xFF;
				textur[i][j][1] = 0xFF;
				textur[i][j][2] = 0xFF;
				textur[i][j][3] = 0x80;	// alpha
			}
		}

		// upload texture to UPLOAD mem and then to GPU mem with "UpdateSubresources"
		subData.pData = textur;
		subData.RowPitch = (LONG_PTR)(4 * desc.Width);
		subData.SlicePitch = desc.Height * subData.RowPitch;

		UpdateSubresources(_ResourceCreationCommandList, m_CursorTexture.Get(), m_UploadHeap.Get(), 0, 0, 1, &subData);


		D3D12_SHADER_RESOURCE_VIEW_DESC textureViewDesc = {};

		textureViewDesc.Format = desc.Format;
		textureViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		textureViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		textureViewDesc.Texture2D.MipLevels = 1;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuTextureHandle(_texHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_texDescriptorSize);	// texture #1 in the heap
		d3dDevice->CreateShaderResourceView(m_CursorTexture.Get(), &textureViewDesc, cpuTextureHandle);

		_ResourceCreationCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CursorTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));	// later, after texture update
	}

	m_LastBlinkTimestamp = 0.0f;	// reset blinking

}






void Cursor::Update(double _CurrentTimestamp, DirectX::XMFLOAT2 _pos)
{
	m_CursorVertices[0] = { XMFLOAT3(_pos.x , _pos.y, 0.0f), XMFLOAT2(0.0f, 0.0f) };
	m_CursorVertices[1] = { XMFLOAT3(_pos.x + m_CursorNormalizedSize.x , _pos.y, 0.0f), XMFLOAT2(1.0f, 0.0f) };
	m_CursorVertices[2] = { XMFLOAT3(_pos.x , _pos.y - m_CursorNormalizedSize.y, 0.0f), XMFLOAT2(0.0f, 1.0f) };
	m_CursorVertices[3] = { XMFLOAT3(_pos.x + m_CursorNormalizedSize.x , _pos.y - m_CursorNormalizedSize.y, 0.0f), XMFLOAT2(1.0f, 1.0f) };

	if (_CurrentTimestamp - m_LastBlinkTimestamp > m_BlinkPeriod)	// _CurrentTimestamp always >= m_LastBlinkTimestamp
	{
		m_IsBlinkVisible = !m_IsBlinkVisible;	// blink period expired, switch blinking
		m_LastBlinkTimestamp = _CurrentTimestamp;
	}
}





void Cursor::ResetBlink(double _CurrentTimestamp)
{
	m_IsBlinkVisible = true;
	m_LastBlinkTimestamp = _CurrentTimestamp;
}







void Cursor::Render(ID3D12DescriptorHeap* _TexHeap, ID3D12GraphicsCommandList * _CommandList)
{
	// show cursor if blinking state is "visible"
	if (m_IsBlinkVisible)
	{
		_CommandList->SetPipelineState(m_pipelineState.Get());

		_CommandList->SetGraphicsRootSignature(m_rootSignature.Get());

		// texture
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(_TexHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_texDescriptorSize);	// descriptor #1
		_CommandList->SetGraphicsRootDescriptorTable(0, gpuHandle);


		_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		_CommandList->IASetVertexBuffers(0, 1, &m_CursorVertexBufferView);
		_CommandList->DrawInstanced(4, 1, 0, 0);
	}
}


}	// of namespace FreeTypeChat