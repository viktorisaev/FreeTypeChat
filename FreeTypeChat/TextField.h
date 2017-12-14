// render screen space grid

#pragma once

#include <ppltasks.h>

#include <vector>

#include "..\Common\DeviceResources.h"
#include "Content/ShaderStructures.h"


namespace FreeTypeChat
{



class Character
{
public:
	Rectangle m_Geom;
	Rectangle m_Texture;
	float m_Baseline;
};


class TextField
{
public:
	TextField();
	~TextField();

	void InitializeTextfield(const std::shared_ptr<DX::DeviceResources>& _DeviceResources);

	void Update();

	void Render(ID3D12GraphicsCommandList *_CommandList);

	void AddCharacter(UINT _Pos, Character _Char);
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
	float m_CharacterRowHeight = 0.16f;
	float m_IntercharacterSpace = 0.007f;
	float m_BaselineCoeff = 0.05f;

};


}
