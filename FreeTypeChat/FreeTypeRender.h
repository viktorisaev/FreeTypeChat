#pragma once

#include <ppltasks.h>

#include "ft2build.h"

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_CACHE_H
#include FT_CACHE_CHARMAP_H
#include FT_CACHE_IMAGE_H
#include FT_CACHE_SMALL_BITMAPS_H
#include FT_SYNTHESIS_H
#include FT_ADVANCES_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_MODULE_H
#include FT_CFF_DRIVER_H
#include FT_TRUETYPE_DRIVER_H
#include FT_LCD_FILTER_H

typedef struct  bcharset_t_
{
	FT_Int     size;
	FT_ULong*  code;

} bcharset_t;


class FreeTypeRender
{
public:
	FreeTypeRender();
	~FreeTypeRender();

	int FreeTypeRender::Initialize(UINT _FontSize);
	Concurrency::task<void> LoadFreeTypeResources();

	byte* GetBitmap();
	std::pair<int, int> GetBitmapSize();
	std::pair<int, int> GetBitmapTopLeft();

	void CreateGlyphBitmap(UINT _charCode);

private:
	void RenderGlyph(FT_Face m_Face, UINT _charCode);
	int test_get_glyph(FT_Face m_Face, void* user_data);
	int test_get_cbox(FT_Face    m_Face, void*      user_data);
	int test_get_bbox(FT_Face    m_Face, void*      user_data);
	int test_get_char_index(FT_Face    m_Face, void*      user_data);
	int test_image_cache(FT_Face    m_Face, void*      user_data);
	void get_charset(FT_Face      m_Face, bcharset_t*  charset);
	FT_Error get_face(FT_Face*  m_Face);


private:
	std::vector<byte>									m_FontData;
	FT_Face   m_Face;

};

