// render screen space grid

#pragma once

#include <ppltasks.h>

#include <vector>

#include "..\Common\DeviceResources.h"
#include "Content/ShaderStructures.h"


namespace FreeTypeChat
{



class CharGlyphInTexture
{
public:
	Rectangle m_Texture;
	float m_Baseline;
};


class Character
{
public:
	Rectangle m_Geom;
	CharGlyphInTexture m_GlyphInTexture;
};



class TextField
{
public:
	TextField();
	~TextField();

	void InitializeTextfield(const std::shared_ptr<DX::DeviceResources>& _DeviceResources, float _LineHeihgtInNormScreen, float _TextureToScreenRatio);

	void Update();

	void Render(ID3D12GraphicsCommandList *_CommandList);

	void AddCharacter(UINT _Pos, CharGlyphInTexture _Char);
	void DeleteCharacter(UINT _Pos);

	UINT GetNumberOfChars() { return m_TextfieldRectangles.size(); }
	DirectX::XMFLOAT2 GetCaretPosByIndex(UINT _pos);	// calculates caret pos by index of a character

private:
	void MapToVertices();

	void RePositionCharacters(UINT _StartPos);	// re-apply positions starting with the char by index


private:

	const int N_CHARS = 1024;	// number of chars in the text field

	//Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
	//Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;

	Microsoft::WRL::ComPtr<ID3D12Resource>				m_TextfieldVertexBuffer;	// of D3D12_HEAP_TYPE_UPLOAD - to CPU write-only, GPU-read-only (or  multiply, depends on cache)
	D3D12_VERTEX_BUFFER_VIEW							m_TextfieldVertexBufferView;
	VertexPositionTexture								*m_TextfieldVertices;	// mapped vertices data
	std::vector<Character>								m_TextfieldRectangles;


	// typing layout
	DirectX::XMFLOAT2 m_BeginCaretPos{ -0.95f, 0.95f };
	float m_CharacterRowHeight = 1.1f;	// coeff of interline interval
	float m_IntercharacterSpace = 0.017f;	// coeff of inter-character space related to m_TextLineHeightInNormScreen
	float m_TextLineHeightInNormScreen;	// height of one line of text in normalized screen coords [-1..1]
	float m_TextureToScreenRatio;	// coeff to convert texture sizes to screen sizes [0..1]  HARDCODED!!!
	float m_BaseLineRatio = 0.83f;	// base line as part of the full char height
	float m_FontAspectRatio = 0.5f;	// depends on output area: multiplier for width against height

};


}
