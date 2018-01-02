#include "pch.h"
#include "CharCache.h"

#include "..\Common\DirectXHelper.h"
#include <ppltasks.h>
#include <synchapi.h>

using namespace FreeTypeChat;

using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;

const int N_GRID_VERT = 50;
const int N_GRID_HORZ = 30;


// Loads vertex and pixel shaders from files and instantiates the cube geometry.
CharCache::CharCache()
{
//	CreateDeviceDependentResources();
//	CreateWindowSizeDependentResources();

	m_NextGlyphPos = DirectX::XMINT2(0, 0);
}

CharCache::~CharCache()
{
}

Concurrency::task<void> FreeTypeChat::CharCache::LoadCharCacheResources()
{
	return m_FreeTypeRender.LoadFreeTypeResources();
}





void CharCache::Initialize(ID3D12Device* _d3dDevice, ID3D12DescriptorHeap *_TexHeap, int _CharCacheTextureDescriptorIndex, UINT _FontSize)
{
	m_FontSize = (float)_FontSize;

	// Create a queue and a command list.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;	// use low-priority queue to update char cache texture
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DX::ThrowIfFailed(_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CharCacheCommandQueue)));
	NAME_D3D12_OBJECT(m_CharCacheCommandQueue);

	DX::ThrowIfFailed(_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CharCacheCommandAllocator)));
	NAME_D3D12_OBJECT(m_CharCacheCommandAllocator);

	DX::ThrowIfFailed(_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CharCacheCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CharCacheCommandList)));
	NAME_D3D12_OBJECT(m_CharCacheCommandList);



	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

	// texture
	{
		D3D12_RESOURCE_DESC desc = {};

		desc.Width = static_cast<UINT>(m_FontTextureSize.x);
		desc.Height = static_cast<UINT>(m_FontTextureSize.y);
		desc.MipLevels = static_cast<UINT16>(1);
		desc.DepthOrArraySize = static_cast<UINT16>(1);
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		DX::ThrowIfFailed(_d3dDevice->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_CharCacheTexture)));
		NAME_D3D12_OBJECT(m_CharCacheTexture);
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_CharCacheTexture.Get(), 0, 1);

		DX::ThrowIfFailed(_d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_CharCacheUploadHeap.GetAddressOf())));
		NAME_D3D12_OBJECT(m_CharCacheUploadHeap);

		D3D12_SUBRESOURCE_DATA subData = {};
		byte *textur = new byte[m_FontTextureSize.x * m_FontTextureSize.y * 4];

		for (int i = 0, ei = m_FontTextureSize.y; i < ei; ++i)
		{
			for (int j = 0, ej = m_FontTextureSize.x; j < ej; ++j)
			{
				textur[(i * m_FontTextureSize.x + j) * 4 + 0] = i * 2;
				textur[(i * m_FontTextureSize.x + j) * 4 + 1] = j * 5;
				textur[(i * m_FontTextureSize.x + j) * 4 + 2] = 0xFF;
				textur[(i * m_FontTextureSize.x + j) * 4 + 3] = (i % 4 == 0) ? 0xFF : (j % 2 == 0) ? 0x80 : 0x00;
			}
		}

		subData.pData = textur;
		subData.RowPitch = 4 * m_FontTextureSize.x;
		subData.SlicePitch = m_FontTextureSize.y * subData.RowPitch;

		UpdateSubresources(m_CharCacheCommandList.Get(), m_CharCacheTexture.Get(), m_CharCacheUploadHeap.Get(), 0, 0, 1, &subData);


		D3D12_SHADER_RESOURCE_VIEW_DESC textureViewDesc = {};

		textureViewDesc.Format = desc.Format;
		textureViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		textureViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		textureViewDesc.Texture2D.MipLevels = 1;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuTextureHandle(_TexHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_cbvDescriptorSize);	// texture #0
		_d3dDevice->CreateShaderResourceView(m_CharCacheTexture.Get(), &textureViewDesc, cpuTextureHandle);

		m_CharCacheCommandList.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CharCacheTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));	// later, after texture update

		delete[] textur;
	}

	m_CharCacheFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	m_CharCacheFenceValue = 0;
	DX::ThrowIfFailed(_d3dDevice->CreateFence(m_CharCacheFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_CharCacheFence)));

	m_FreeTypeRender.Initialize((UINT)m_FontSize);

	// transfer
	DX::ThrowIfFailed(m_CharCacheCommandList.Get()->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CharCacheCommandList.Get() };
	m_CharCacheCommandQueue.Get()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGpu();	// wait for the texture transfer to complete
}








// TODO: do not wait GPU on main render
GlyphInTexture CharCache::UpdateTexture(UINT charCode)
{

	// check if glyph already there
	std::vector<GlyphInTexture>::const_iterator ut = m_FreeTypeCacheVector.end();
	{
//		std::lock_guard<std::mutex> locker_cache(m_FreeTypeCacheMutex);		// we are the only writers to the cache, so reading could be without lock. Check lock on writing below.

		GlyphInTexture curGlyph(charCode);

		ut = std::lower_bound(m_FreeTypeCacheVector.begin(), m_FreeTypeCacheVector.end(), curGlyph);

		if (ut != m_FreeTypeCacheVector.end())
		{
			if (ut->m_CharCode == charCode)
			{
				return GlyphInTexture();
			}
		}
	}



	m_FreeTypeRender.CreateGlyphBitmap(charCode);

	std::pair<int, int> bitmapSize = m_FreeTypeRender.GetBitmapSize();
	std::pair<int, int> bitmapOffset = m_FreeTypeRender.GetBitmapTopLeft();

	int w = bitmapSize.first;// 20 + (rand() * 50) / (RAND_MAX + 1);
	int h = bitmapSize.second;// 20 + (rand() * 50) / (RAND_MAX + 1);

	DirectX::XMINT2 nextPos = DirectX::XMINT2(m_NextGlyphPos.x + w, m_NextGlyphPos.y);

	if (nextPos.x > m_FontTextureSize.x)
	{
		// new line
		nextPos.x = 0;
		nextPos.y += (int32_t)m_FontSize;

		m_NextGlyphPos = nextPos;

		nextPos.x += w;	// at least one glyph should have space in one row
	}

	if (m_NextGlyphPos.y + h > m_FontTextureSize.y)
	{
		return GlyphInTexture();	// TODO: out of vertical texture size. Add texture?
	}

	int x = m_NextGlyphPos.x;
	int y = m_NextGlyphPos.y;

	m_NextGlyphPos = nextPos;


	// update texture
	DX::ThrowIfFailed(m_CharCacheCommandList->Reset(m_CharCacheCommandAllocator.Get(), nullptr));

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
	copyLocation.pResource = m_CharCacheTexture.Get();

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLocation.PlacedFootprint.Footprint.Width = m_FontTextureSize.x;
	srcLocation.PlacedFootprint.Footprint.Height = m_FontTextureSize.y;
	srcLocation.PlacedFootprint.Footprint.Depth = 1;
	srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srcLocation.PlacedFootprint.Footprint.RowPitch = srcLocation.PlacedFootprint.Footprint.Width * 4;
	srcLocation.PlacedFootprint.Offset = 0;
	srcLocation.pResource = m_CharCacheUploadHeap.Get();

	BYTE* pData;
	DX::ThrowIfFailed(m_CharCacheUploadHeap->Map(0, NULL, reinterpret_cast<void**>(&pData)));

	byte *glyph = m_FreeTypeRender.GetBitmap();

	byte valr = 0xFF;

	for (int i = 0, ei = h; i < ei; ++i)
	{
		for (int j = 0, ej = w; j < ej; ++j)
		{
			pData[((y+i) * m_FontTextureSize.x + x+j) * 4 + 0] = valr;
			pData[((y+i) * m_FontTextureSize.x + x+j) * 4 + 1] = valr;
			pData[((y+i) * m_FontTextureSize.x + x+j) * 4 + 2] = valr;
			pData[((y+i) * m_FontTextureSize.x + x+j) * 4 + 3] = glyph[i*w+j];
		}
	}

	m_CharCacheUploadHeap->Unmap(0, NULL);

	PIXBeginEvent(m_CharCacheCommandList.Get(), 0, L"UpdateTexture");

	m_CharCacheCommandList.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CharCacheTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));

	m_CharCacheCommandList.Get()->CopyTextureRegion(&copyLocation, x, y, 0, &srcLocation, &box);

	m_CharCacheCommandList.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CharCacheTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	PIXEndEvent(m_CharCacheCommandList.Get());	// UpdateTexture


	DX::ThrowIfFailed(m_CharCacheCommandList.Get()->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CharCacheCommandList.Get() };
	m_CharCacheCommandQueue.Get()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	WaitForGpu();	// wait for the texture transfer to complete

	// add item to cache
	{
		std::lock_guard<std::mutex> locker_cache(m_FreeTypeCacheMutex);

		Rectangle texCoord(DirectX::XMFLOAT2((float)x / (float)m_FontTextureSize.x, (float)y / (float)m_FontTextureSize.y), DirectX::XMFLOAT2((float)w / (float)m_FontTextureSize.x, (float)h / (float)m_FontTextureSize.y));

		GlyphInTexture curGlyph(charCode, texCoord, bitmapOffset.first / m_FontSize);	// TODO: use font parameters here

		m_FreeTypeCacheVector.insert(ut, curGlyph);

		return curGlyph;
	}	// mem release semantic

}





// Wait for pending GPU work to complete.
void CharCache::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	DX::ThrowIfFailed(m_CharCacheCommandQueue.Get()->Signal(m_CharCacheFence.Get(), m_CharCacheFenceValue));

	// Wait until the fence has been crossed.
	DX::ThrowIfFailed(m_CharCacheFence->SetEventOnCompletion(m_CharCacheFenceValue, m_CharCacheFenceEvent));
	WaitForSingleObjectEx(m_CharCacheFenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_CharCacheFenceValue++;
}







bool CharCache::GetGlyph(UINT charCode, GlyphInTexture & _Glyph)
{
	{
		std::lock_guard<std::mutex> locker_cache(m_FreeTypeCacheMutex);

		GlyphInTexture curGlyph(charCode);
		std::vector<GlyphInTexture>::const_iterator ut = std::lower_bound(m_FreeTypeCacheVector.begin(), m_FreeTypeCacheVector.end(), curGlyph);

		if (ut != m_FreeTypeCacheVector.end())
		{
			if (ut->m_CharCode == charCode)
			{
				_Glyph = *ut;
				return true;
			}
		}

		return false;
	}
}






DirectX::XMINT2 CharCache::GetFontTextureSize()
{
	return m_FontTextureSize;
}
