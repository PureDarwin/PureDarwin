/* wld: font.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "wld-private.h"

#include <fontconfig/fcfreetype.h>

EXPORT
struct wld_font_context *
wld_font_create_context()
{
	struct wld_font_context *context;

	context = malloc(sizeof *context);

	if (!context)
		goto error0;

	if (FT_Init_FreeType(&context->library) != 0) {
		DEBUG("Failed to initialize FreeType library\n");

		goto error1;
	}

	return context;

error1:
	free(context);
error0:
	return NULL;
}

EXPORT
void
wld_font_destroy_context(struct wld_font_context *context)
{
	FT_Done_FreeType(context->library);
	free(context);
}

EXPORT
struct wld_font *
wld_font_open_name(struct wld_font_context *context, const char *name)
{
	FcPattern *pattern, *match;
	FcResult result;

	DEBUG("Opening font with name: %s\n", name);

	pattern = FcNameParse((const FcChar8 *)name);
	FcConfigSubstitute(NULL, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	match = FcFontMatch(NULL, pattern, &result);

	if (!match)
		return NULL;

	return wld_font_open_pattern(context, match);
}

EXPORT
struct wld_font *
wld_font_open_pattern(struct wld_font_context *context, FcPattern *match)
{
	char *filename;
	struct font *font;
	FcResult result;
	double pixel_size, aspect;

	font = malloc(sizeof *font);

	if (!font)
		goto error0;

	font->context = context;

	result = FcPatternGetString(match, FC_FILE, 0, (FcChar8 **)&filename);

	if (result == FcResultMatch) {
		FT_Error error;

		DEBUG("Loading font file: %s\n", filename);

		error = FT_New_Face(context->library, filename, 0, &font->face);

		if (error == 0)
			goto load_face;
	}

	result = FcPatternGetFTFace(match, FC_FT_FACE, 0, &font->face);

	if (result != FcResultMatch) {
		DEBUG("Couldn't determine font filename or FreeType face\n");
		goto error1;
	}

load_face:
	result = FcPatternGetDouble(match, FC_PIXEL_SIZE, 0, &pixel_size);

	result = FcPatternGetDouble(match, FC_ASPECT, 0, &aspect);

	if (result == FcResultNoMatch)
		aspect = 1.0;

	if (font->face->face_flags & FT_FACE_FLAG_SCALABLE) {
		FT_F26Dot6 width, height;

		width = ((unsigned int)pixel_size) << 6;
		height = ((unsigned int)(pixel_size * aspect)) << 6;

		FT_Set_Char_Size(font->face, width, height, 0, 0);
	} else {
		FT_Set_Pixel_Sizes(font->face, (unsigned int)pixel_size,
		                   (unsigned int)(pixel_size * aspect));
	}

	font->base.ascent = font->face->size->metrics.ascender >> 6;
	font->base.descent = -font->face->size->metrics.descender >> 6;
	font->base.height = font->base.ascent + font->base.descent;
	font->base.max_advance = font->face->size->metrics.max_advance >> 6;

	font->glyphs = calloc(font->face->num_glyphs, sizeof(struct glyph *));

	return &font->base;

error1:
	free(font);
error0:
	return NULL;
}

EXPORT
void
wld_font_close(struct wld_font *font_base)
{
	struct font *font = (void *)font_base;

	FT_Done_Face(font->face);
	free(font);
}

bool
font_ensure_glyph(struct font *font, FT_UInt glyph_index)
{
	if (glyph_index) {
		if (!font->glyphs[glyph_index]) {
			struct glyph *glyph;

			glyph = malloc(sizeof *glyph);

			if (!glyph)
				return false;

			FT_Load_Glyph(font->face, glyph_index,
			              FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);

			FT_Bitmap_New(&glyph->bitmap);

			FT_Bitmap_Copy(font->context->library,
			               &font->face->glyph->bitmap, &glyph->bitmap);

			glyph->advance = font->face->glyph->metrics.horiAdvance >> 6;
			glyph->x = font->face->glyph->bitmap_left;
			glyph->y = -font->face->glyph->bitmap_top;

			font->glyphs[glyph_index] = glyph;
		}

		return true;
	}

	return false;
}

EXPORT
bool
wld_font_ensure_char(struct wld_font *font_base, uint32_t character)
{
	struct font *font = (void *)font_base;
	FT_UInt glyph_index;

	glyph_index = FT_Get_Char_Index(font->face, character);

	return font_ensure_glyph(font, glyph_index);
}

EXPORT
void
wld_font_text_extents_n(struct wld_font *font_base,
                        const char *text, int32_t length,
                        struct wld_extents *extents)
{
	struct font *font = (void *)font_base;
	int ret;
	uint32_t c;
	FT_UInt glyph_index;

	extents->advance = 0;

	while ((ret = FcUtf8ToUcs4((FcChar8 *)text, &c, length)) > 0 && c != '\0') {
		length -= ret;
		text += ret;
		glyph_index = FT_Get_Char_Index(font->face, c);

		if (!font_ensure_glyph(font, glyph_index))
			continue;

		extents->advance += font->glyphs[glyph_index]->advance;
	}
}
