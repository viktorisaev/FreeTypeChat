#include "pch.h"
#include "FreeTypeRender.h"

#include "..\Common\DirectXHelper.h"


FreeTypeRender::FreeTypeRender()
{
}


FreeTypeRender::~FreeTypeRender()
{
	//if (cache_man)
	//  FTC_Manager_Done(cache_man);

//	FT_Done_FreeType(lib);
}





static FT_Error get_face(FT_Face*  m_Face);


/*
* Globals
*/

#define CACHE_SIZE  1024


static FT_Library        lib;
static FTC_Manager       cache_man;
static FTC_CMapCache     cmap_cache;
static FTC_ImageCache    image_cache;
static FTC_SBitCache     sbit_cache;
static FTC_ImageTypeRec  font_type;


enum {
	FT_BENCH_LOAD_GLYPH,
	FT_BENCH_LOAD_ADVANCES,
	FT_BENCH_RENDER,
	FT_BENCH_GET_GLYPH,
	FT_BENCH_GET_CBOX,
	FT_BENCH_CMAP,
	FT_BENCH_CMAP_ITER,
	FT_BENCH_NEW_FACE,
	FT_BENCH_EMBOLDEN,
	FT_BENCH_GET_BBOX,
	N_FT_BENCH
};


static const char*  bench_desc[] =
{
	"load a glyph        (FT_Load_Glyph)",
	"load advance widths (FT_Get_Advances)",
	"render a glyph      (FT_Render_Glyph)",
	"load a glyph        (FT_Get_Glyph)",
	"get glyph cbox      (FT_Glyph_Get_CBox)",
	"get glyph indices   (FT_Get_Char_Index)",
	"iterate CMap        (FT_Get_{First,Next}_Char)",
	"open a new face     (FT_New_Face)",
	"embolden            (FT_GlyphSlot_Embolden)",
	"get glyph bbox      (FT_Outline_Get_BBox)",
	NULL
};


static int    preload;
static char*  filename;

static UINT  first_index;

static FT_Render_Mode  render_mode = FT_RENDER_MODE_NORMAL;
static FT_Int32        load_flags = FT_LOAD_DEFAULT;

static UINT  tt_interpreter_versions[3];
static int   num_tt_interpreter_versions;
static UINT  dflt_tt_interpreter_version;

static UINT  cff_hinting_engines[2];
static int   num_cff_hinting_engines;
static UINT  dflt_cff_hinting_engine;

static char  cff_hinting_engine_names[2][10] = { "freetype", "adobe" };


static FT_Error face_requester(FTC_FaceID face_id, FT_Library library, FT_Pointer request_data, FT_Face* aface)
{
	FT_UNUSED(face_id);
	FT_UNUSED(library);

	*aface = (FT_Face)request_data;

	return FT_Err_Ok;
}


void FreeTypeRender::CreateGlyphBitmap(UINT _charCode)
{
	RenderGlyph(m_Face, _charCode);
}



void FreeTypeRender::RenderGlyph(FT_Face m_Face, UINT _charCode)
{
	FT_Error error = FT_Load_Char(m_Face, _charCode, FT_LOAD_MONOCHROME);
	error = FT_Render_Glyph(m_Face->glyph, FT_RENDER_MODE_NORMAL);
	// then call GetBitmap to receive alpha matrix
}


int FreeTypeRender::test_get_glyph(FT_Face m_Face, void* user_data)
{
	FT_Glyph      glyph;
	UINT  i;
	int           done = 0;

	FT_UNUSED(user_data);


	for (i = first_index; i < (UINT)m_Face->num_glyphs; i++)
	{
		if (FT_Load_Glyph(m_Face, i, load_flags))
			continue;

		if (!FT_Get_Glyph(m_Face->glyph, &glyph))
		{
			FT_Done_Glyph(glyph);
			done++;
		}
	}

	return done;
}


int FreeTypeRender::test_get_cbox(FT_Face    m_Face, void*      user_data)
{
	FT_Glyph      glyph;
	FT_BBox       bbox;
	UINT  i;
	int           done = 0;

	FT_UNUSED(user_data);


	for (i = first_index; i < (UINT)m_Face->num_glyphs; i++)
	{
		if (FT_Load_Glyph(m_Face, i, load_flags))
			continue;

		if (FT_Get_Glyph(m_Face->glyph, &glyph))
			continue;

		FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);

		FT_Done_Glyph(glyph);
		done++;
	}

	return done;
}


int FreeTypeRender::test_get_bbox(FT_Face m_Face, void* user_data)
{
	FT_BBox       bbox;
	UINT  i;
	int           done = 0;
	FT_Matrix     rot30 = { 0xDDB4, -0x8000, 0x8000, 0xDDB4 };

	FT_UNUSED(user_data);


	for (i = first_index; i < (UINT)m_Face->num_glyphs; i++)
	{
		FT_Outline*  outline;


		if (FT_Load_Glyph(m_Face, i, load_flags))
			continue;

		outline = &m_Face->glyph->outline;

		/* rotate outline by 30 degrees */
		FT_Outline_Transform(outline, &rot30);

		FT_Outline_Get_BBox(outline, &bbox);

		done++;
	}

	return done;
}


int FreeTypeRender::test_get_char_index(FT_Face m_Face,	void* user_data)
{
	bcharset_t*  charset = (bcharset_t*)user_data;
	int          i, done = 0;


	for (i = 0; i < charset->size; i++)
	{
		if (FT_Get_Char_Index(m_Face, charset->code[i]))
			done++;
	}

	return done;
}


int FreeTypeRender::test_image_cache(FT_Face m_Face, void* user_data)
{
	FT_Glyph      glyph;
	UINT  i;
	int           done = 0;

	FT_UNUSED(user_data);


	if (!image_cache)
	{
		if (FTC_ImageCache_New(cache_man, &image_cache))
			return 0;
	}

	for (i = first_index; i < (UINT)m_Face->num_glyphs; i++)
	{
		if (!FTC_ImageCache_Lookup(image_cache, &font_type, i, &glyph, NULL))
		{
			done++;
		}
	}

	return done;
}


/*
* main
*/

void FreeTypeRender::get_charset(FT_Face m_Face, bcharset_t*  charset)
{
	FT_ULong  charcode;
	FT_UInt   gindex;
	int       i;


	charset->code = (FT_ULong*)calloc((size_t)m_Face->num_glyphs, sizeof(FT_ULong));
	if (!charset->code)
		return;

	if (m_Face->charmap)
	{
		i = 0;
		charcode = FT_Get_First_Char(m_Face, &gindex);

		/* certain fonts contain a broken charmap that will map character */
		/* codes to out-of-bounds glyph indices.  Take care of that here. */
		/*                                                                */
		while (gindex && i < m_Face->num_glyphs)
		{
			if (gindex >= first_index)
			{
				charset->code[i++] = charcode;
			}
			charcode = FT_Get_Next_Char(m_Face, charcode, &gindex);
		}
	}
	else
	{
		UINT  j;


		/* no charmap, do an identity mapping */
		for (i = 0, j = first_index; j < (UINT)m_Face->num_glyphs; i++, j++)
		{
			charset->code[i] = j;
		}
	}

	charset->size = i;
}


FT_Error FreeTypeRender::get_face(FT_Face*  m_Face)
{
	static unsigned char*  memory_file = NULL;
	static size_t          memory_size;
	int                    face_index = 0;
	FT_Error               error;


	if (preload)
	{
		error = FT_New_Memory_Face(lib,	m_FontData.data(), (FT_Long)m_FontData.size(), face_index, m_Face);
	}
	else
		error = FT_New_Face(lib, filename, face_index, m_Face);

	if (error)
		fprintf(stderr, "couldn't load font resource\n");

	return error;
}


byte* FreeTypeRender::GetBitmap()
{
	return m_Face->glyph->bitmap.buffer;
}


std::pair<int, int> FreeTypeRender::GetBitmapSize()
{
	return std::make_pair(m_Face->glyph->bitmap.width, m_Face->glyph->bitmap.rows);
}


std::pair<int, int> FreeTypeRender::GetBitmapTopLeft()
{
	return std::make_pair(m_Face->glyph->bitmap_top, m_Face->glyph->bitmap_left);
}


//static void
//usage(void)
//{
//	int   i;
//	char  interpreter_versions[32];
//	char  hinting_engines[32];
//
//
//	/* we expect that at least one interpreter version is available */
//	if (num_tt_interpreter_versions == 2)
//		sprintf_s(interpreter_versions,
//			"%d and %d",
//			tt_interpreter_versions[0],
//			tt_interpreter_versions[1]);
//	else
//		sprintf_s(interpreter_versions,
//			"%d, %d, and %d",
//			tt_interpreter_versions[0],
//			tt_interpreter_versions[1],
//			tt_interpreter_versions[2]);
//
//	/* we expect that at least one hinting engine is available */
//	if (num_cff_hinting_engines == 1)
//		sprintf_s(hinting_engines,
//			"`%s'",
//			cff_hinting_engine_names[cff_hinting_engines[0]]);
//	else
//		sprintf_s(hinting_engines,
//			"`%s' and `%s'",
//			cff_hinting_engine_names[cff_hinting_engines[0]],
//			cff_hinting_engine_names[cff_hinting_engines[1]]);
//
//
//	fprintf(stderr,
//		"\n"
//		"ftbench: run FreeType benchmarks\n"
//		"--------------------------------\n"
//		"\n"
//		"Usage: ftbench [options] fontname\n"
//		"\n"
//		"  -C        Compare with cached version (if available).\n"
//		"  -c N      Use at most N iterations for each test\n"
//		"            (0 means time limited).\n"
//		"  -f L      Use hex number L as load flags (see `FT_LOAD_XXX').\n"
//		"  -H NAME   Use CFF hinting engine NAME.\n"
//		"            Available versions are %s; default is `%s'.\n"
//		"  -I VER    Use TT interpreter version VER.\n"
//		"            Available versions are %s; default is version %d.\n"
//		"  -i IDX    Start with index IDX (default is 0).\n"
//		"  -l N      Set LCD filter to N\n"
//		"              0: none, 1: default, 2: light, 16: legacy\n"
//		"  -m M      Set maximum cache size to M KiByte (default is %d).\n",
//		hinting_engines,
//		cff_hinting_engine_names[dflt_cff_hinting_engine],
//		interpreter_versions,
//		dflt_tt_interpreter_version,
//		CACHE_SIZE);
//	fprintf(stderr,
//		"  -p        Preload font file in memory.\n"
//		"  -r N      Set render mode to N\n"
//		"              0: normal, 1: light, 2: mono, 3: LCD, 4: LCD vertical\n"
//		"            (default is 0).\n"
//		"  -s S      Use S ppem as face size (default is %dppem).\n"
//		"            If set to zero, don't call FT_Set_Pixel_Sizes.\n"
//		"            Use value 0 with option `-f 1' or something similar to\n"
//		"            load the glyphs unscaled, otherwise errors will show up.\n",
//		FACE_SIZE);
//	fprintf(stderr,
//		"  -t T      Use at most T seconds per bench (default is %.0f).\n"
//		"\n"
//		"  -b tests  Perform chosen tests (default is all):\n",
//		BENCH_TIME);
//
//	for (i = 0; i < N_FT_BENCH; i++)
//	{
//		if (!bench_desc[i])
//			break;
//
//		fprintf(stderr,
//			"              %c  %s\n", 'a' + i, bench_desc[i]);
//	}
//
//	fprintf(stderr,
//		"\n"
//		"  -v        Show version.\n"
//		"\n");
//
//	exit(1);
//}



int FreeTypeRender::Initialize(UINT _FontSize)
{
	FT_Error  error;

	unsigned long  max_bytes = CACHE_SIZE * 1024;
	char*          test_string = NULL;
	int            max_iter = 0;
	int            compare_cached = 0;
	int            j;

	UINT versions[3] = { TT_INTERPRETER_VERSION_35, TT_INTERPRETER_VERSION_38, TT_INTERPRETER_VERSION_40 };
	UINT  engines[2] = { FT_CFF_HINTING_FREETYPE, FT_CFF_HINTING_ADOBE };
	int version;
	char *engine;


	if (FT_Init_FreeType(&lib))
	{
		fprintf(stderr, "could not initialize font library\n");

		return 1;
	}


	// collect all available versions, then set again the default
	FT_Property_Get(lib, "truetype", "interpreter-version", &dflt_tt_interpreter_version);

	for (j = 0; j < 3; j++)
	{
		error = FT_Property_Set(lib, "truetype", "interpreter-version", &versions[j]);
		if (!error)
		{
			tt_interpreter_versions[num_tt_interpreter_versions++] = versions[j];
		}
	}
	FT_Property_Set(lib, "truetype", "interpreter-version", &dflt_tt_interpreter_version);

	FT_Property_Get(lib, "cff", "hinting-engine", &dflt_cff_hinting_engine);

	for (j = 0; j < 2; j++)
	{
		error = FT_Property_Set(lib, "cff", "hinting-engine", &engines[j]);
		if (!error)
		{
			cff_hinting_engines[num_cff_hinting_engines++] = engines[j];
		}
	}
	FT_Property_Set(lib, "cff", "hinting-engine", &dflt_cff_hinting_engine);

	version = (int)dflt_tt_interpreter_version;
	engine = cff_hinting_engine_names[dflt_cff_hinting_engine];

//	while (1)
//	{
//		int  opt;
//
//
//		opt = 'b';//getopt(argc, argv, "b:Cc:f:H:I:i:l:m:pr:s:t:v");
//
//		if (opt == -1)
//			break;
//
//		switch (opt)
//		{
//		case 'b':
////			test_string = optarg;
//			break;
//
//		case 'C':
//			compare_cached = 1;
//			break;
//
//		case 'c':
////			max_iter = atoi(optarg);
//			if (max_iter < 0)
//				max_iter = -max_iter;
//			break;
//
//		case 'f':
////			load_flags = strtol(optarg, NULL, 16);
//			break;
//
//		case 'H':
////			engine = optarg;
//
//			for (j = 0; j < num_cff_hinting_engines; j++)
//			{
//				if (!strcmp(engine, cff_hinting_engine_names[j]))
//				{
//					FT_Property_Set(lib,
//						"cff",
//						"hinting-engine", &j);
//					break;
//				}
//			}
//
//			if (j == num_cff_hinting_engines)
//				fprintf(stderr,
//					"warning: couldn't set CFF hinting engine\n");
//			break;
//
//		case 'I':
////			version = atoi(optarg);
//
//			for (j = 0; j < num_tt_interpreter_versions; j++)
//			{
//				if (version == (int)tt_interpreter_versions[j])
//				{
//					FT_Property_Set(lib,
//						"truetype",
//						"interpreter-version", &version);
//					break;
//				}
//			}
//
//			if (j == num_tt_interpreter_versions)
//				fprintf(stderr,
//					"warning: couldn't set TT interpreter version\n");
//			break;
//
//		case 'i':
//		{
////			int  fi = atoi(optarg);
//
//
//			//if (fi > 0)
//			//	first_index = (UINT)fi;
//		}
//		break;
//
//		case 'l':
//		{
////			int  filter = atoi(optarg);
//
//
//			//switch (filter)
//			//{
//			//case FT_LCD_FILTER_NONE:
//			//case FT_LCD_FILTER_DEFAULT:
//			//case FT_LCD_FILTER_LIGHT:
//			//case FT_LCD_FILTER_LEGACY1:
//			//case FT_LCD_FILTER_LEGACY:
//			//	FT_Library_SetLcdFilter(lib, (FT_LcdFilter)filter);
//			//}
//		}
//		break;
//
//		case 'm':
//		{
////			int  mb = atoi(optarg);
//
//
//			//if (mb > 0)
//			//	max_bytes = (UINT)mb * 1024;
//		}
//		break;
//
//		case 'p':
//			preload = 1;
//			break;
//
//		case 'r':
//		{
////			int  rm = atoi(optarg);
//
//
//			//if (rm < 0 || rm >= FT_RENDER_MODE_MAX)
//			//	render_mode = FT_RENDER_MODE_NORMAL;
//			//else
//			//	render_mode = (FT_Render_Mode)rm;
//		}
//		break;
//
//		case 's':
//		{
////			int  sz = atoi(optarg);
//
//
//			/* value 0 is special */
//			//if (sz < 0)
//			//	size = 1;
//			//else
//			//	size = (UINT)sz;
//		}
//		break;
//
//		case 't':
////			max_time = atof(optarg);
//			if (max_time < 0)
//				max_time = -max_time;
//			break;
//
//		case 'v':
//		{
//			FT_Int  major, minor, patch;
//
//
//			FT_Library_Version(lib, &major, &minor, &patch);
//
//			printf("ftbench (FreeType) %d.%d", major, minor);
//			if (patch)
//				printf(".%d", patch);
//			printf("\n");
//			exit(0);
//		}
//		/* break; */
//
//		default:
//			break;
//		}
//	}

//	argc -= optind;
//	argv += optind;

	filename = "arial.ttf";
	preload = 1;

	if (get_face(&m_Face))
		goto Exit;



	if (_FontSize)
	{
		if (FT_IS_SCALABLE(m_Face))
		{
			if (FT_Set_Pixel_Sizes(m_Face, _FontSize, _FontSize))
			{
				fprintf(stderr, "failed to set pixel size to %d\n", _FontSize);

				return 1;
			}
		}
		else
		{
			_FontSize = (UINT)m_Face->available_sizes[0].size >> 6;
			fprintf(stderr,
				"using size of first bitmap strike (%dpx)\n", _FontSize);
			FT_Select_Size(m_Face, 0);
		}
	}

	FTC_Manager_New(lib, 0, 0, max_bytes, face_requester, m_Face, &cache_man);

	font_type.face_id = (FTC_FaceID)1;
	font_type.width = _FontSize;
	font_type.height = _FontSize;
	font_type.flags = FT_RENDER_MODE_MONO;//load_flags;

	//printf("\n"
	//	"ftbench results for font `%s'\n"
	//	"---------------------------",
	//	filename);
	//for (i = 0; i < strlen(filename); i++)
	//	putchar('-');
	//putchar('\n');

	//printf("\n"
	//	"family: %s\n"
	//	" style: %s\n"
	//	"\n",
	//	m_Face->family_name,
	//	m_Face->style_name);

	//if (max_iter)
	//	printf("number of iterations for each test: at most %d\n",
	//		max_iter);
	//printf("number of seconds for each test: %s%f\n",
	//	max_iter ? "at most " : "",
	//	max_time);

	//printf("\n"
	//	"starting glyph index: %d\n"
	//	"face size: %dppem\n"
	//	"font preloading into memory: %s\n",
	//	first_index,
	//	size,
	//	preload ? "yes" : "no");

	//printf("\n"
	//	"load flags: 0x%X\n"
	//	"render mode: %d\n",
	//	load_flags,
	//	render_mode);
	//printf("\n"
	//	"CFF hinting engine set to `%s'\n"
	//	"TrueType interpreter set to version %d\n"
	//	"maximum cache size: %ldKiByte\n",
	//	engine,
	//	version,
	//	max_bytes / 1024);

	//printf("\n"
	//	"executing tests:\n");

	// DEBUG: render a char at init
//	test_render(m_Face, nullptr, 0x00BE);

Exit:
	/* The following is a bit subtle: When we call FTC_Manager_Done, this
	* normally destroys all FT_Face objects that the cache might have
	* created by calling the face requester.
	*
	* However, this little benchmark uses a tricky face requester that
	* doesn't create a new FT_Face through FT_New_Face but simply passes a
	* pointer to the one that was previously created.
	*
	* If the cache manager has been used before, the call to
	* FTC_Manager_Done discards our single FT_Face.
	*
	* In the case where no cache manager is in place, or if no test was
	* run, the call to FT_Done_FreeType releases any remaining FT_Face
	* object anyway.
	*/

	return 0;
}

Concurrency::task<void> FreeTypeRender::LoadFreeTypeResources()
{
	auto loadFontTask = DX::ReadDataAsync(L"arial.ttf").then([this](std::vector<byte>& fileData)
	{
		m_FontData = fileData;
	});

	return loadFontTask;
}


/* End */

