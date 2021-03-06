#include "pch.h"

#include "TextField.h"
#include "..\Common\DirectXHelper.h"


namespace FreeTypeChat
{

using namespace DirectX;


TextField::TextField() :
	m_TextfieldRectangles()
{
	m_TextfieldRectangles.reserve(N_CHARS);
}


TextField::~TextField()
{
}




void TextField::InitializeTextfield(const std::shared_ptr<DX::DeviceResources>& _DeviceResources, float _LineHeihgtInNormScreen, float _TextureToScreenRatio)
{
	m_TextLineHeightInNormScreen = _LineHeihgtInNormScreen;
	m_TextureToScreenRatio = _TextureToScreenRatio;

	// Create and upload cube geometry resources to the GPU.
	auto d3dDevice = _DeviceResources->GetD3DDevice();

	// Vertices.

	const UINT textfieldVerticesBufferSize = N_CHARS * (4+2) * sizeof(VertexPositionTexture);

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



void TextField::AddCharacter(UINT _IdxInString, CharGlyphInTexture _GlyphInTexture)
{
	Character c;
	c.m_GlyphInTexture = _GlyphInTexture;
	// do not set position, it is set in "RePositionCharacters"

	m_TextfieldRectangles.insert(m_TextfieldRectangles.begin() +_IdxInString, c);

	RePositionCharacters(_IdxInString);
}






void TextField::DeleteCharacter(UINT _Pos)
{
	m_TextfieldRectangles.erase(m_TextfieldRectangles.begin() + _Pos);	// TODO; fix artefact that type vertext buffer could be being rendered at the time of modification

	RePositionCharacters(_Pos);
}




void TextField::RePositionCharacters(UINT /*_StartPos*/)
{
//	DirectX::XMFLOAT2 nextPos = _StartPos > 0 ? m_TextfieldRectangles[_StartPos - 1].m_Geom.m_Pos : m_BeginCaretPos;	// start from top left
	DirectX::XMFLOAT2 nextPos = DirectX::XMFLOAT2(m_BeginCaretPos.x, m_BeginCaretPos.y - m_TextLineHeightInNormScreen * m_BaseLineRatio);	// start from top left

	float interCharNorm = m_IntercharacterSpace * m_TextLineHeightInNormScreen;

	for (UINT i = /*_StartPos > 0 ? _StartPos-1 : */0, ei = GetNumberOfChars(); i < ei; ++i)
	{
		DirectX::XMFLOAT2 pos = nextPos;

		Character curChar = m_TextfieldRectangles[i];
		float w = curChar.m_GlyphInTexture.m_Texture.m_Size.x * m_TextureToScreenRatio * m_TextLineHeightInNormScreen * m_FontAspectRatio;
		float h = curChar.m_GlyphInTexture.m_Texture.m_Size.y * m_TextureToScreenRatio * m_TextLineHeightInNormScreen;

		nextPos.x += (interCharNorm + w);

		if (nextPos.x > 0)	// text field width reached, move to the next line
		{
			nextPos.x = m_BeginCaretPos.x;
			nextPos.y -= m_TextLineHeightInNormScreen * m_CharacterRowHeight;

			pos = nextPos;

			nextPos.x += (interCharNorm + w);
		}

		// baseline
		pos.y += m_TextLineHeightInNormScreen * curChar.m_GlyphInTexture.m_Baseline;	// move up from baseline

		m_TextfieldRectangles[i].m_Geom = Rectangle(pos, DirectX::XMFLOAT2(w,h));

	}

	MapToVertices();
}






void TextField::MapToVertices()
{
	VertexPositionTexture *vertices = m_TextfieldVertices;

	for (int i = 0, ei = GetNumberOfChars(); i < ei; ++i)
	{
		Character &chr = m_TextfieldRectangles[i];

		float dx = chr.m_Geom.m_Pos.x + chr.m_Geom.m_Size.x;
		float dy = chr.m_Geom.m_Pos.y - chr.m_Geom.m_Size.y;

		const Rectangle &glyphTex = chr.m_GlyphInTexture.m_Texture;

		*vertices = { DirectX::XMFLOAT3(chr.m_Geom.m_Pos.x, chr.m_Geom.m_Pos.y, 0.0f), DirectX::XMFLOAT2(glyphTex.m_Pos.x, glyphTex.m_Pos.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(dx, chr.m_Geom.m_Pos.y, 0.0f), DirectX::XMFLOAT2(glyphTex.m_Pos.x + glyphTex.m_Size.x, glyphTex.m_Pos.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(chr.m_Geom.m_Pos.x, dy, 0.0f), DirectX::XMFLOAT2(glyphTex.m_Pos.x, glyphTex.m_Pos.y + glyphTex.m_Size.y) };
		++vertices;
		*vertices = { DirectX::XMFLOAT3(dx, dy, 0.0f), DirectX::XMFLOAT2(glyphTex.m_Pos.x + glyphTex.m_Size.x, glyphTex.m_Pos.y + glyphTex.m_Size.y) };
		++vertices;

		if (i < ei - 1)	// add gap like 2 empty triangles
		{
			// previous
			*vertices = { DirectX::XMFLOAT3(dx, dy, 0.0f), DirectX::XMFLOAT2(glyphTex.m_Pos.x + glyphTex.m_Size.x, glyphTex.m_Pos.y + glyphTex.m_Size.y) };
			++vertices;
			// next
			Character &rectNext = m_TextfieldRectangles[i+1];
			*vertices = { DirectX::XMFLOAT3(rectNext.m_Geom.m_Pos.x, rectNext.m_Geom.m_Pos.y, 0.0f), DirectX::XMFLOAT2(rectNext.m_GlyphInTexture.m_Texture.m_Pos.x, rectNext.m_GlyphInTexture.m_Texture.m_Pos.y) };
			++vertices;
		}
	}
}







DirectX::XMFLOAT2 TextField::GetCaretPosByIndex(UINT _pos)
{
	DirectX::XMFLOAT2 curPos = m_BeginCaretPos;	// start from top left corner

	float interCharNorm = m_IntercharacterSpace * m_TextLineHeightInNormScreen;

	for (UINT i = 0, ei = _pos; i < ei; ++i)
	{
		Character curChar = m_TextfieldRectangles[i];
		float w = curChar.m_Geom.m_Size.x;
		float h = curChar.m_Geom.m_Size.y;

		curPos.x += (interCharNorm + w);

		if (curPos.x > 0)	// text field width reached, move to the next line
		{
			curPos.x = m_BeginCaretPos.x + (interCharNorm + w);
			curPos.y -= m_TextLineHeightInNormScreen * m_CharacterRowHeight;
		}
	}

	// correct to next char (if not last)
	if (_pos < GetNumberOfChars())
	{
		Character curChar = m_TextfieldRectangles[_pos];
		float w = curChar.m_Geom.m_Size.x;
		float h = curChar.m_Geom.m_Size.y;

		if (curPos.x + (interCharNorm + w) > 0)	// text field width reached, move to the next line
		{
			curPos.x = m_BeginCaretPos.x;
			curPos.y -= m_TextLineHeightInNormScreen * m_CharacterRowHeight;
		}
	}

	return curPos;
}








void TextField::Render(ID3D12GraphicsCommandList * _CommandList)
{
//	_CommandList->SetPipelineState(m_pipelineState.Get());

//	_CommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);		// TODO: use triangle strip with 2 degenerate triangles between characters. Possible improvement: geometry shader.
	_CommandList->IASetVertexBuffers(0, 1, &m_TextfieldVertexBufferView);
	UINT n = GetNumberOfChars();
	_CommandList->DrawInstanced((n * 4) + (n > 0 ? (n-1)*2 : 0), 1, 0, 0);
}











}	// of namespace FreeTypeChat