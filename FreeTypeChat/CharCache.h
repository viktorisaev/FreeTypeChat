#pragma once

#include "Common/DeviceResources.h"
#include "Content/ShaderStructures.h"
#include "FreeTypeRender.h"


namespace FreeTypeChat
{

	// glyph in texture: char, baseline, geometry (position, size)
	class GlyphInTexture
	{
	public:
		GlyphInTexture() :
			m_CharCode(0)
			, m_TexCoord()
			, m_Baseline(0.0f)
		{}

		GlyphInTexture(UINT _CharCode, Rectangle _TexCoord, float _Baseline) :
			m_CharCode(_CharCode)
			, m_TexCoord(_TexCoord)
			, m_Baseline(_Baseline)
		{}

		GlyphInTexture(UINT _CharCode) :
			  m_CharCode(_CharCode)
			, m_TexCoord()
			, m_Baseline(0.0f)
		{}

		bool operator<(const GlyphInTexture& _other)
		{
			return this->m_CharCode < _other.m_CharCode;
		}


	public:
		UINT m_CharCode;
		Rectangle m_TexCoord;
		float m_Baseline;
	};





	class CharCache
	{
	public:
		CharCache();
		~CharCache();
		Concurrency::task<void> LoadCharCacheResources();
		void Initialize(ID3D12Device* _d3dDevice, ID3D12DescriptorHeap *_TexHeap, int _CharCacheTextureDescriptorIndex, UINT _FontSize);
//		void CreateWindowSizeDependentResources();
//		CD3DX12_GPU_DESCRIPTOR_HANDLE SetHeapAndGetGPUHandleForCharCacheTexture(ID3D12GraphicsCommandList *_CommandList);	// HEAVY! SetDescriptorHeaps
		GlyphInTexture UpdateTexture(UINT charCode);

		bool GetGlyph(UINT charCode, GlyphInTexture& _Glyph);

		DirectX::XMINT2 GetFontTextureSize();


	private:	// func
		void WaitForGpu();

	private:	// data

		FreeTypeRender m_FreeTypeRender;	// free type renderer

		const DirectX::XMINT2 m_FontTextureSize { 512, 512 };


		// Constant buffers must be 256-byte aligned.
//		static const UINT c_alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_CharCacheCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_CharCacheCommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_CharCacheCommandList;

		Microsoft::WRL::ComPtr<ID3D12Fence>				m_CharCacheFence;
		UINT64											m_CharCacheFenceValue;
		HANDLE											m_CharCacheFenceEvent;


		Microsoft::WRL::ComPtr<ID3D12Resource>				m_CharCacheTexture;
		UINT												m_cbvDescriptorSize;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_CharCacheUploadHeap;

		DirectX::XMINT2		m_NextGlyphPos;

		std::mutex m_FreeTypeCacheMutex;
		std::vector<GlyphInTexture> m_FreeTypeCacheVector;				// current characters cache (all used characters)
	};
}

