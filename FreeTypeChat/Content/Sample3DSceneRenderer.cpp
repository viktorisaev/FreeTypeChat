#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"
#include <ppltasks.h>
#include <synchapi.h>

using namespace FreeTypeChat;

using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;

// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

const int N_GRID_VERT = 50;
const int N_GRID_HORZ = 30;


// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_radiansPerSecond(XM_PIDIV4),	// rotate 45 degrees per second
	m_angle(0),
//	m_mappedConstantBuffer(nullptr),
	m_deviceResources(deviceResources)
{
	LoadState();
//	ZeroMemory(&m_constantBufferData, sizeof(m_constantBufferData));

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

Sample3DSceneRenderer::~Sample3DSceneRenderer()
{
//	m_constantBuffer->Unmap(0, nullptr);
//	m_mappedConstantBuffer = nullptr;
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	auto d3dDevice = m_deviceResources->GetD3DDevice();

	// Create a root signature with a single constant buffer slot.
	{
		CD3DX12_DESCRIPTOR_RANGE range;
		CD3DX12_ROOT_PARAMETER parameter;

		// texture
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		parameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);

		// sampler for the texture
		D3D12_STATIC_SAMPLER_DESC samplerRing = {};
		samplerRing.Filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;//D3D12_FILTER_MIN_MAG_MIP_LINEAR;
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

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		//|	D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;	// allow pixel

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(1, &parameter, 1, &samplerRing, rootSignatureFlags);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        NAME_D3D12_OBJECT(m_rootSignature);
	}

	auto createVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso").then([this](std::vector<byte>& fileData) {
		m_vertexShader = fileData;
	});

	auto createPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso").then([this](std::vector<byte>& fileData) {
		m_pixelShader = fileData;
	});

	// Create the pipeline state once the shaders are loaded.
	auto createPipelineStateTask = (createPSTask && createVSTask).then([this]() {

		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = m_rootSignature.Get();
        state.VS = CD3DX12_SHADER_BYTECODE(&m_vertexShader[0], m_vertexShader.size());
        state.PS = CD3DX12_SHADER_BYTECODE(&m_pixelShader[0], m_pixelShader.size());
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		state.DepthStencilState.DepthEnable = false;											// DISABLE DEPTH TEST!!!
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
		state.DSVFormat = m_deviceResources->GetDepthBufferFormat();
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

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_pipelineState)));


		m_Cursor.InitializeCursorPipelineState(m_deviceResources, state.VS, state.PS);

		// Shader data can be deleted once the pipeline state is created.
		m_vertexShader.clear();
		m_pixelShader.clear();
	});


	auto createGridTask = m_ScreenGrid.LoadGridResources(m_deviceResources);

	auto createFreeTypeTask = m_FreeTypeRender.LoadFreeTypeResources();



	// Create and upload cube geometry resources to the GPU.
	auto createAssetsTask = (createPipelineStateTask && createGridTask && createFreeTypeTask).then([this]() {
		auto d3dDevice = m_deviceResources->GetD3DDevice();

		// Create a command list.
		DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), nullptr, IID_PPV_ARGS(&m_commandList)));
        NAME_D3D12_OBJECT(m_commandList);





		// Cube vertices. Each vertex has a position and a color.
		VertexPositionTexture cubeVertices[] =
		{
			{ XMFLOAT3( 0.0f,  1.0f,  0.0f), XMFLOAT2(0.0f, 0.0f) },
			{ XMFLOAT3( 0.0f, -1.0f,  0.0f), XMFLOAT2(0.0f, 1.0f) },
			{ XMFLOAT3( 1.0f,  1.0f,  0.0f), XMFLOAT2(1.0f, 0.0f) },
			{ XMFLOAT3( 1.0f, -1.0f,  0.0f), XMFLOAT2(1.0f, 1.0f) },
		};

		const UINT vertexBufferSize = sizeof(cubeVertices);

		// Create the vertex buffer resource in the GPU's default heap and copy vertex data into it using the upload heap.
		// The upload resource must not be released until after the GPU has finished using it.
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUpload;

		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBufferUpload)));

        NAME_D3D12_OBJECT(m_vertexBuffer);

		// Upload the vertex buffer to the GPU.
		{
			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = reinterpret_cast<BYTE*>(cubeVertices);
			vertexData.RowPitch = vertexBufferSize;
			vertexData.SlicePitch = vertexData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_vertexBuffer.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		}


		// Load mesh indices. Each trio of indices represents a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes 0, 2 and 1 from the vertex buffer compose the
		// first triangle of this mesh.
		unsigned short cubeIndices[] =
		{
			0, 2, 1,
			1, 2, 3,
		};

		const UINT indexBufferSize = sizeof(cubeIndices);

		// Create the index buffer resource in the GPU's default heap and copy index data into it using the upload heap.
		// The upload resource must not be released until after the GPU has finished using it.
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUpload;

		CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_indexBuffer)));

		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBufferUpload)));

		NAME_D3D12_OBJECT(m_indexBuffer);

		// Upload the index buffer to the GPU.
		{
			D3D12_SUBRESOURCE_DATA indexData = {};
			indexData.pData = reinterpret_cast<BYTE*>(cubeIndices);
			indexData.RowPitch = indexBufferSize;
			indexData.SlicePitch = indexData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_indexBuffer.Get(), indexBufferUpload.Get(), 0, 0, 1, &indexData);

			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
		}



		// Create a descriptor heap for the textures (ALL).
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = 2;	// texture #0=font, texture#1=cursor			// DX::c_frameCount;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_texHeap)));

            NAME_D3D12_OBJECT(m_texHeap);
		}

		m_cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		//CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(DX::c_frameCount * c_alignedConstantBufferSize);
		//DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		//	&uploadHeapProperties,
		//	D3D12_HEAP_FLAG_NONE,
		//	&constantBufferDesc,
		//	D3D12_RESOURCE_STATE_GENERIC_READ,
		//	nullptr,
		//	IID_PPV_ARGS(&m_constantBuffer)));

  //      NAME_D3D12_OBJECT(m_constantBuffer);

		//// Create constant buffer views to access the upload buffer.
		//D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_constantBuffer->GetGPUVirtualAddress();
		//CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(m_texHeap->GetCPUDescriptorHandleForHeapStart());

		//for (int n = 0; n < DX::c_frameCount; n++)
		//{
		//	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		//	desc.BufferLocation = cbvGpuAddress;
		//	desc.SizeInBytes = c_alignedConstantBufferSize;
		//	d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);

		//	cbvGpuAddress += desc.SizeInBytes;
		//	cbvCpuHandle.Offset(m_cbvDescriptorSize);
		//}

		//// Map the constant buffers.
		//CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
		//DX::ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedConstantBuffer)));
		//ZeroMemory(m_mappedConstantBuffer, DX::c_frameCount * c_alignedConstantBufferSize);
		//// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.



		// texture
		{
			D3D12_RESOURCE_DESC desc = {};

			desc.Width = static_cast<UINT>(m_Width);
			desc.Height = static_cast<UINT>(m_Height);
			desc.MipLevels = static_cast<UINT16>(1);
			desc.DepthOrArraySize = static_cast<UINT16>(1);
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_Texture)));
			NAME_D3D12_OBJECT(m_Texture);
			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_Texture.Get(), 0, 1);

			DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_UploadHeap.GetAddressOf())));
			NAME_D3D12_OBJECT(m_UploadHeap);

			D3D12_SUBRESOURCE_DATA subData = {};
			byte *textur = new byte[m_Width * m_Height * 4];

			for (int i = 0, ei = m_Height; i < ei; ++i)
			{
				for (int j = 0, ej = m_Width; j < ej; ++j)
				{
					textur[(i * m_Width + j) * 4 + 0] = i * 3;
					textur[(i * m_Width + j) * 4 + 1] = j * 3;
					textur[(i * m_Width + j) * 4 + 2] = 0xFF;
					textur[(i * m_Width + j) * 4 + 3] = (i % 4 == 0) ? 0xFF : (j % 2 == 0) ? 0x80 : 0x00;
				}
			}

			subData.pData = textur;
			subData.RowPitch = 4 * m_Width;
			subData.SlicePitch = m_Height * subData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_Texture.Get(), m_UploadHeap.Get(), 0, 0, 1, &subData);


			D3D12_SHADER_RESOURCE_VIEW_DESC textureViewDesc = {};

			textureViewDesc.Format = desc.Format;
			textureViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			textureViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			textureViewDesc.Texture2D.MipLevels = 1;

			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuTextureHandle(m_texHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_cbvDescriptorSize);	// texture #0
			d3dDevice->CreateShaderResourceView(m_Texture.Get(), &textureViewDesc, cpuTextureHandle);

//			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));	// later, after texture update

			delete[] textur;
		}

		// Create vertex/index buffer views.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(VertexPositionTexture);
		m_vertexBufferView.SizeInBytes = sizeof(cubeVertices);

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.SizeInBytes = sizeof(cubeIndices);
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;

		// init grid
		m_ScreenGrid.InitializeGrid(m_deviceResources, m_commandList.Get(), N_GRID_VERT, N_GRID_HORZ);

		// cursor
		m_Cursor.InitializeCursor(m_deviceResources, m_texHeap.Get(), m_commandList.Get(), DirectX::XMFLOAT2(0.01f, 0.15f), 0.5 );

		// text field
		m_TextField.InitializeTextfield(m_deviceResources);

		// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
		DX::ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


		// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
		m_deviceResources->WaitForGpu();

		m_FreeTypeRender.mmmmmmmmain();


		UpdateTexture();



		m_CaretPos = DirectX::XMFLOAT2(m_LeftTextfieldSide, 0.95f);


	});


	createAssetsTask.then([this]() {
		m_loadingComplete = true;
	});
}








void FreeTypeChat::Sample3DSceneRenderer::UpdateTexture()
{

	std::pair<int, int> bitmapSize = m_FreeTypeRender.GetBitmapSize();

	int w = bitmapSize.first;// 20 + (rand() * 50) / (RAND_MAX + 1);
	int h = bitmapSize.second;// 20 + (rand() * 50) / (RAND_MAX + 1);

	int x = (rand() * (m_Width - w)) / (RAND_MAX + 1);
	int y = (rand() * (m_Height - h)) / (RAND_MAX + 1);


	// update texture
	DX::ThrowIfFailed(m_commandList->Reset(m_deviceResources->GetCommandAllocator(), m_pipelineState.Get()));
	{
		D3D12_BOX box = {};
		box.left = x;
		box.top = y;
		box.front = 0;

		box.right = x + w;
		box.bottom = y + h;
		box.back = 1;


		D3D12_TEXTURE_COPY_LOCATION copyLocation = {};
		copyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		copyLocation.SubresourceIndex = 0;
		copyLocation.pResource = m_Texture.Get();

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLocation.PlacedFootprint.Footprint.Width = m_Width;
		srcLocation.PlacedFootprint.Footprint.Height = m_Height;
		srcLocation.PlacedFootprint.Footprint.Depth = 1;
		srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srcLocation.PlacedFootprint.Footprint.RowPitch = srcLocation.PlacedFootprint.Footprint.Width * 4;
		srcLocation.PlacedFootprint.Offset = 0;
		srcLocation.pResource = m_UploadHeap.Get();

		BYTE* pData;
		DX::ThrowIfFailed(m_UploadHeap->Map(0, NULL, reinterpret_cast<void**>(&pData)));

		byte *glyph = m_FreeTypeRender.GetBitmap();

		byte valr = 10 + (rand() * (243)) / (RAND_MAX + 1);
		byte valg = 10 + (rand() * (243)) / (RAND_MAX + 1);
		byte valb = 10 + (rand() * (243)) / (RAND_MAX + 1);
		byte mult = 0 + (rand() * (255)) / (RAND_MAX + 1);

		for (int i = 0, ei = h; i < ei; ++i)
		{
			for (int j = 0, ej = w; j < ej; ++j)
			{
				pData[((y+i) * m_Width + x+j) * 4 + 0] = valr;
				pData[((y+i) * m_Width + x+j) * 4 + 1] = valg;
				pData[((y+i) * m_Width + x+j) * 4 + 2] = valb;
				pData[((y+i) * m_Width + x+j) * 4 + 3] = glyph[i*w+j];
			}
		}

		m_UploadHeap->Unmap(0, NULL);

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));

		m_commandList->CopyTextureRegion(&copyLocation, x, y, 0, &srcLocation, &box);
	}

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	DX::ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	ppCommandLists[0] = m_commandList.Get();
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_deviceResources->WaitForGpu();
}






// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	//// This sample makes use of a right-handed coordinate system using row-major matrices.
	//XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
	//	fovAngleY,
	//	aspectRatio,
	//	0.01f,
	//	100.0f
	//	);

	//XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();
	//XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	//XMStoreFloat4x4(
	//	&m_constantBufferData.projection,
	//	XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
	//	);

	//// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	//static const XMVECTORF32 eye = { 0.0f, 0.7f, 1.5f, 0.0f };
	//static const XMVECTORF32 at = { 0.0f, -0.1f, 0.0f, 0.0f };
	//static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	//XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}








// Called once per frame.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (m_loadingComplete)
	{
		// Rotate the cube a small amount.
		m_angle += static_cast<float>(timer.GetElapsedSeconds()) * m_radiansPerSecond;

		Rotate(m_angle);

		// Update the constant buffer resource.
		//UINT8* destination = m_mappedConstantBuffer + (m_deviceResources->GetCurrentFrameIndex() * c_alignedConstantBufferSize);
		//memcpy(destination, &m_constantBufferData, sizeof(m_constantBufferData));

		m_Cursor.Update(timer.GetTotalSeconds(), DirectX::XMFLOAT2(m_CaretPos.x, m_CaretPos.y + 0.03f));

	}
}







// TODO: add char to output
void Sample3DSceneRenderer::AddChar()
{
	if (m_loadingComplete)
	{
		UpdateTexture();

		// type to textfield
		float w = 0.03f + ((rand() * 60) / (RAND_MAX + 1)) / 1000.0f;	// [0.1..0.15)
		float h = 0.07f + ((rand() * 60) / (RAND_MAX + 1)) / 1000.0f;	// [0.1..0.12)

		DirectX::XMFLOAT2 curPos = m_CaretPos;
		m_CaretPos.x += (m_IntercharacterSpace + w);

		if (m_CaretPos.x > 0)	// text field width reached, move to the next line
		{
			m_CaretPos.x = m_LeftTextfieldSide;
			m_CaretPos.y -= m_CharacterRowHeight;

			curPos = m_CaretPos;

			m_CaretPos.x += (m_IntercharacterSpace + w);

		}

		Character c =
		{
			Rectangle(DirectX::XMFLOAT2(curPos.x, curPos.y), DirectX::XMFLOAT2(w, h)),
			Rectangle(DirectX::XMFLOAT2(0.05f, ((rand() * 600) / (RAND_MAX + 1)) / 1000.0f), DirectX::XMFLOAT2(w, h))
		};

		m_TextField.AddCharacter(c);
	}
}








// Saves the current state of the renderer.
void Sample3DSceneRenderer::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(AngleKey))
	{
		state->Remove(AngleKey);
	}
	if (state->HasKey(TrackingKey))
	{
		state->Remove(TrackingKey);
	}

	state->Insert(AngleKey, PropertyValue::CreateSingle(m_angle));
}

// Restores the previous state of the renderer.
void Sample3DSceneRenderer::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(AngleKey))
	{
		m_angle = safe_cast<IPropertyValue^>(state->Lookup(AngleKey))->GetSingle();
		state->Remove(AngleKey);
	}
	if (state->HasKey(TrackingKey))
	{
		state->Remove(TrackingKey);
	}
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader.
//	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
}








// Renders one frame using the vertex and pixel shaders.
bool Sample3DSceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return false;
	}

	DX::ThrowIfFailed(m_deviceResources->GetCommandAllocator()->Reset());

	// The command list can be reset anytime after ExecuteCommandList() is called.
	DX::ThrowIfFailed(m_commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr));

	PIXBeginEvent(m_commandList.Get(), 0, L"DrawFrame");

	// Set the viewport and scissor rectangle.
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate this resource will be in use as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Record drawing commands.
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
	m_commandList->ClearRenderTargetView(renderTargetView, DirectX::Colors::DarkSlateBlue, 0, nullptr);
	//		m_commandList->ClearRenderTargetView(renderTargetView, m_deviceResources->GetCurrentFrameIndex() % DX::c_frameCount == 0 ? DirectX::Colors::Green : DirectX::Colors::Red, 0, nullptr);
	m_commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);


	PIXBeginEvent(m_commandList.Get(), 0, L"DrawGrid");
	m_ScreenGrid.Render(m_commandList.Get());
	PIXEndEvent(m_commandList.Get());	// cube


	PIXBeginEvent(m_commandList.Get(), 0, L"Draw the cube");
	{

		m_commandList->SetPipelineState(m_pipelineState.Get());

		// Set the graphics root signature and descriptor heaps to be used by this frame.
		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		ID3D12DescriptorHeap* ppHeaps[] = { m_texHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);	// HEAVYY!!!

		// texture
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_texHeap->GetGPUDescriptorHandleForHeapStart(), 0/*m_deviceResources->GetCurrentFrameIndex()*/, m_cbvDescriptorSize);
		m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);

		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

		m_commandList->IASetIndexBuffer(&m_indexBufferView);
		m_commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}
	PIXEndEvent(m_commandList.Get());	// cube

	// text field. Reuse pipeline and texture
	m_TextField.Render(m_commandList.Get());

	// cursor
	{
		// heap is already setup
		m_Cursor.Render(m_texHeap.Get(), m_commandList.Get());
	}



	// Indicate that the render target will now be used to present when the command list is done executing.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	PIXEndEvent(m_commandList.Get());	// frame

	DX::ThrowIfFailed(m_commandList->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	return true;
}
