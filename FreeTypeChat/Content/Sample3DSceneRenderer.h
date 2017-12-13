#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"
#include "ScreenGrid.h"
#include "FreeTypeRender.h"
#include "Cursor.h"
#include "TextField.h"

namespace FreeTypeChat
{


	class GlyphInTexture
	{
	public:
		GlyphInTexture() :
		  m_CharCode(0)
		, m_TexCoord()
		{}

		GlyphInTexture(UINT _CharCode, Rectangle _TexCoord) :
			m_CharCode(_CharCode)
			, m_TexCoord(_TexCoord)
		{}

		GlyphInTexture(UINT _CharCode) :
			m_CharCode(_CharCode)
			, m_TexCoord()
		{}

		bool operator<(const GlyphInTexture& _other)
		{
			return this->m_CharCode < _other.m_CharCode;
		}


	public:
		UINT m_CharCode;
		Rectangle m_TexCoord;
	};



	// This sample renderer instantiates a basic rendering pipeline.
	class Sample3DSceneRenderer
	{
	public:
		Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~Sample3DSceneRenderer();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void Update(DX::StepTimer const& timer);
		GlyphInTexture AddCharToCache(UINT charCode);
		bool Render();
		void SaveState();

		bool IsLoadingComplete() { return m_loadingComplete; }

		Cursor&	GetCursor() { return m_Cursor; }
		TextField&	GetTextfield() { return m_TextField; }

		bool GetGlyph(UINT charCode, GlyphInTexture& _Glyph);
		DirectX::XMINT2 GetFontTextureSize();

	private:
		void LoadState();
		void Rotate(float radians);
		GlyphInTexture UpdateTexture(UINT charCode);

	private:

		const DirectX::XMINT2 m_FontTextureSize { 512, 512 };


		// Constant buffers must be 256-byte aligned.
//		static const UINT c_alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_texHeap;	// Main resource heap

		Microsoft::WRL::ComPtr<ID3D12Resource>				m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_indexBuffer;

		Microsoft::WRL::ComPtr<ID3D12Resource>				m_Texture;

//		Microsoft::WRL::ComPtr<ID3D12Resource>				m_constantBuffer;
//		ModelViewProjectionConstantBuffer					m_constantBufferData;
//		UINT8*												m_mappedConstantBuffer;
		UINT												m_cbvDescriptorSize;

		D3D12_RECT											m_scissorRect;
		std::vector<byte>									m_vertexShader;
		std::vector<byte>									m_pixelShader;
		D3D12_VERTEX_BUFFER_VIEW							m_vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW								m_indexBufferView;

		Microsoft::WRL::ComPtr<ID3D12Resource>				m_UploadHeap;

		Microsoft::WRL::ComPtr<ID3D12Resource>				m_GridVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW							m_GridVertexBufferView;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_radiansPerSecond;
		float	m_angle;

		ScreenGrid m_ScreenGrid;
		Cursor		m_Cursor;
		TextField	m_TextField;
		FreeTypeRender m_FreeTypeRender;

		// TODO: move to glyph texture render
		DirectX::XMINT2		m_NextGlyphPos;

		std::mutex m_FreeTypeCacheMutex;
		std::vector<GlyphInTexture> m_FreeTypeCacheVector;				// current characters cache (all used characters)

	};
}

