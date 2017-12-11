// render screen space grid

#pragma once

#include <ppltasks.h>

#include "..\Common\DeviceResources.h"
#include "Content/ShaderStructures.h"


namespace FreeTypeChat
{

class Cursor
{
public:
	Cursor();
	~Cursor();

	void InitializeCursorPipelineState(std::shared_ptr<DX::DeviceResources> _DeviceResources, const D3D12_SHADER_BYTECODE &_VertexShader, const D3D12_SHADER_BYTECODE &_PixelShader);
	void InitializeCursor(const std::shared_ptr<DX::DeviceResources>& _DeviceResources, ID3D12DescriptorHeap* _texHeap, ID3D12GraphicsCommandList *_ResourceCreationCommandList, DirectX::XMFLOAT2	_CursorNormalizedSize, double _BlinkPeriod);

	void Update(double _CurrentTimestamp, DirectX::XMFLOAT2 _pos);	// set cursor to position
	void ResetBlink(double _CurrentTimestamp);

	void Render(ID3D12DescriptorHeap* _texHeap, ID3D12GraphicsCommandList *_CommandList);

private:


	Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;

	Microsoft::WRL::ComPtr<ID3D12Resource>				m_CursorVertexBuffer;	// of D3D12_HEAP_TYPE_UPLOAD - to CPU write-only, GPU-read-only (or  multiply, depends on cache)
	D3D12_VERTEX_BUFFER_VIEW							m_CursorVertexBufferView;
	VertexPositionTexture								*m_CursorVertices;	// mapped vertices data

	DirectX::XMFLOAT2	m_CursorNormalizedSize;		// size of the cursor in normalized screen space [-1..+1]

	Microsoft::WRL::ComPtr<ID3D12Resource>				m_CursorTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource>				m_UploadHeap;


	UINT												m_texDescriptorSize;

	double												m_BlinkPeriod;
	double												m_LastBlinkTimestamp;	// in seconds
	bool												m_IsBlinkVisible;		// current state of blinking: true=visible, false=hidden. Switches every period

};


}
