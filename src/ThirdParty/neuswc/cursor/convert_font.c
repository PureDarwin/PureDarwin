/*
 * Copyright © 2012 Philipp Brüschweiler
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * This is a small, hacky tool to extract cursors from a .pcf file.
 * The information about the file format has been gathered from
 * http://fontforge.org/pcf-format.html
 */

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

struct glyph {
	char *name;
	int16_t left_bearing, right_bearing, ascent, descent;

	int16_t width, height;
	int16_t hotx, hoty;

	int32_t data_format;
	char *data;
};

static struct {
	int count;
	struct glyph *glyphs;
} extracted_font = {0, NULL};

#define PCF_PROPERTIES (1 << 0)
#define PCF_ACCELERATORS (1 << 1)
#define PCF_METRICS (1 << 2)
#define PCF_BITMAPS (1 << 3)
#define PCF_INK_METRICS (1 << 4)
#define PCF_BDF_ENCODINGS (1 << 5)
#define PCF_SWIDTHS (1 << 6)
#define PCF_GLYPH_NAMES (1 << 7)
#define PCF_BDF_ACCELERATORS (1 << 8)

#define PCF_DEFAULT_FORMAT 0x00000000
#define PCF_INKBOUNDS 0x00000200
#define PCF_ACCEL_W_INKBOUNDS 0x00000100
#define PCF_COMPRESSED_METRICS 0x00000100

#define PCF_FORMAT_MASK 0xffffff00

struct pcf_header {
	char header[4];
	int32_t table_count;
	struct toc_entry {
		int32_t type;
		int32_t format;
		int32_t size;
		int32_t offset;
	} tables[];
};

struct compressed_metrics {
	uint8_t left_sided_bearing;
	uint8_t right_side_bearing;
	uint8_t character_width;
	uint8_t character_ascent;
	uint8_t character_descent;
};

struct uncompressed_metrics {
	int16_t left_sided_bearing;
	int16_t right_side_bearing;
	int16_t character_width;
	int16_t character_ascent;
	int16_t character_descent;
	uint16_t character_attributes;
};

struct metrics {
	int32_t format;
	union {
		struct {
			int16_t count;
			struct compressed_metrics compressed_metrics[];
		} compressed;
		struct {
			int32_t count;
			struct uncompressed_metrics uncompressed_metrics[];
		} uncompressed;
	};
};

struct glyph_names {
	int32_t format;
	int32_t glyph_count;
	int32_t offsets[];
};

struct bitmaps {
	int32_t format;
	int32_t glyph_count;
	int32_t offsets[];
};

static void
handle_compressed_metrics(int32_t count, struct compressed_metrics *m)
{
#if ENABLE_DEBUG
	fprintf(stderr, "metrics count: %d\n", count);
#endif
	extracted_font.count = count;
	extracted_font.glyphs = calloc(count, sizeof(struct glyph));

	int i;
	for (i = 0; i < count; ++i) {
		struct glyph *glyph = &extracted_font.glyphs[i];
		glyph->left_bearing = ((int16_t)m[i].left_sided_bearing) - 0x80;
		glyph->right_bearing = ((int16_t)m[i].right_side_bearing) - 0x80;
		glyph->width = ((int16_t)m[i].character_width) - 0x80;
		glyph->ascent = ((int16_t)m[i].character_ascent) - 0x80;
		glyph->descent = ((int16_t)m[i].character_descent) - 0x80;

		/* computed stuff */
		glyph->height = glyph->ascent + glyph->descent;

		glyph->hotx = -glyph->left_bearing;
		glyph->hoty = glyph->ascent;
	}
}

static void
handle_metrics(void *metricbuf)
{
	struct metrics *metrics = metricbuf;
#if ENABLE_DEBUG
	fprintf(stderr, "metric format: %x\n", metrics->format);
#endif

	if ((metrics->format & PCF_FORMAT_MASK) == PCF_DEFAULT_FORMAT) {
#if ENABLE_DEBUG
		fprintf(stderr, "todo...\n");
#endif
	} else if ((metrics->format & PCF_FORMAT_MASK) == PCF_COMPRESSED_METRICS) {
		handle_compressed_metrics(metrics->compressed.count,
		                          &metrics->compressed.compressed_metrics[0]);
	} else {
#if ENABLE_DEBUG
		fprintf(stderr, "incompatible format\n");
#endif
		abort();
	}
}

static void
handle_glyph_names(struct glyph_names *names)
{
#if ENABLE_DEBUG
	fprintf(stderr, "glyph count %d\n", names->glyph_count);
#endif

	if (names->glyph_count != extracted_font.count) {
		abort();
	}

#if ENABLE_DEBUG
	fprintf(stderr, "glyph names format %x\n", names->format);
#endif

	char *names_start = ((char *)names) + sizeof(struct glyph_names) +
	                    (names->glyph_count + 1) * sizeof(int32_t);

	int i;
	for (i = 0; i < names->glyph_count; ++i) {
		int32_t start = names->offsets[i];
		int32_t end = names->offsets[i + 1];
		char *name = names_start + start;
		extracted_font.glyphs[i].name = calloc(1, end - start + 1);
		memcpy(extracted_font.glyphs[i].name, name, end - start);
	}
}

static void
handle_bitmaps(struct bitmaps *bitmaps)
{
#if ENABLE_DEBUG
	fprintf(stderr, "bitmaps count %d\n", bitmaps->glyph_count);
#endif

	if (bitmaps->glyph_count != extracted_font.count) {
		abort();
	}
#if ENABLE_DEBUG
	fprintf(stderr, "format %x\n", bitmaps->format);
#endif

	if (bitmaps->format != 2) {
#if ENABLE_DEBUG
		fprintf(stderr, "format not yet supported\n");
#endif
		abort();
	}

	char *bitmaps_start = ((char *)bitmaps) + sizeof(struct bitmaps) +
	                      (bitmaps->glyph_count + 4) * sizeof(int32_t);

	for (unsigned i = 0; i < bitmaps->glyph_count; ++i) {
		int32_t offset = bitmaps->offsets[i];
		struct glyph *glyph = &extracted_font.glyphs[i];
		glyph->data_format = bitmaps->format;

		glyph->data = bitmaps_start + offset;
	}
}

static void
handle_pcf(void *fontbuf)
{
	struct pcf_header *header = fontbuf;
#if ENABLE_DEBUG
	fprintf(stderr, "tablecount %d\n", header->table_count);
#endif

	for (unsigned i = 0; i < header->table_count; ++i) {
		struct toc_entry *entry = &header->tables[i];
#if ENABLE_DEBUG
		fprintf(stderr, "type: %d\n", entry->type);
#endif
		if (entry->type == PCF_METRICS) {
			handle_metrics((void *)((uintptr_t)fontbuf + entry->offset));
		} else if (entry->type == PCF_GLYPH_NAMES) {
			handle_glyph_names((void *)((uintptr_t)fontbuf + entry->offset));
		} else if (entry->type == PCF_BITMAPS) {
			handle_bitmaps((void *)((uintptr_t)fontbuf + entry->offset));
		}
	}
}

static char
get_glyph_pixel(struct glyph *glyph, int x, int y)
{
	int absx = glyph->hotx + x;
	int absy = glyph->hoty + y;

	if (absx < 0 || absx >= glyph->width || absy < 0 || absy >= glyph->height) {
		return 0;
	}

	int stride = (glyph->width + 31) / 32 * 4;
	unsigned char block = glyph->data[absy * stride + (absx / 8)];
	int idx = absx % 8;
	return (block >> idx) & 1;
}

static struct {
	uint32_t *data;
	size_t capacity, size;
} data_buffer;

static void
init_data_buffer(void)
{
	data_buffer.data = malloc(sizeof(uint32_t) * 10);
	data_buffer.capacity = 10;
	data_buffer.size = 0;
}

static void
add_pixel(uint32_t pixel)
{
	if (data_buffer.size == data_buffer.capacity) {
		data_buffer.capacity *= 2;
		data_buffer.data =
		    realloc(data_buffer.data, sizeof(uint32_t) * data_buffer.capacity);
	}
	data_buffer.data[data_buffer.size++] = pixel;
}

struct reconstructed_glyph {
	int32_t width, height;
	int32_t hotspot_x, hotspot_y;
	size_t offset;
	char *name;
};

static void
reconstruct_glyph(struct glyph *cursor, struct glyph *mask, char *name,
                  struct reconstructed_glyph *glyph)
{
	int minx = min(-cursor->hotx, -mask->hotx);
	int maxx = max(cursor->right_bearing, mask->right_bearing);

	int miny = min(-cursor->hoty, -mask->hoty);
	int maxy = max(cursor->height - cursor->hoty, mask->height - mask->hoty);

	int width = maxx - minx;
	int height = maxy - miny;

	glyph->name = strdup(name);
	glyph->width = width;
	glyph->height = height;
	glyph->hotspot_x = -minx;
	glyph->hotspot_y = -miny;
	glyph->offset = data_buffer.size;

	int x, y;
	for (y = miny; y < maxy; ++y) {
		for (x = minx; x < maxx; ++x) {
			char alpha = get_glyph_pixel(mask, x, y);
			if (alpha) {
				char color = get_glyph_pixel(cursor, x, y);
				if (color) {
					add_pixel(0xff000000);
				} else {
					add_pixel(0xffffffff);
				}
			} else {
				add_pixel(0);
			}
		}
	}
}

/* From http://cgit.freedesktop.org/xorg/lib/libXfont/tree/src/builtins/fonts.c
 */
static const char cursor_licence[] =
    "/*\n"
    "* Copyright 1999 SuSE, Inc.\n"
    "*\n"
    "* Permission to use, copy, modify, distribute, and sell this software and "
    "its\n"
    "* documentation for any purpose is hereby granted without fee, provided "
    "that\n"
    "* the above copyright notice appear in all copies and that both that\n"
    "* copyright notice and this permission notice appear in supporting\n"
    "* documentation, and that the name of SuSE not be used in advertising or\n"
    "* publicity pertaining to distribution of the software without specific,\n"
    "* written prior permission.  SuSE makes no representations about the\n"
    "* suitability of this software for any purpose.  It is provided \"as "
    "is\"\n"
    "* without express or implied warranty.\n"
    "*\n"
    "* SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING "
    "ALL\n"
    "* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL "
    "SuSE\n"
    "* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY "
    "DAMAGES\n"
    "* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN "
    "ACTION\n"
    "* OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN\n"
    "* CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
    "*\n"
    "* Author:  Keith Packard, SuSE, Inc.\n"
    "*/\n";

static void
write_output_file(FILE *file, struct reconstructed_glyph *glyphs, int n)
{
	int i, j, counter, size;
	uint32_t *data;

	fprintf(file, "%s\n", cursor_licence);

	fprintf(file, "static uint32_t cursor_data[] = {\n\t");

	counter = 0;
	for (i = 0; i < n; ++i) {
		data = data_buffer.data + glyphs[i].offset;
		size = glyphs[i].width * glyphs[i].height;

		for (j = 0; j < size; ++j) {
			fprintf(file, "0x%08x, ", data[j]);
			if (++counter % 6 == 0) {
				fprintf(file, "\n\t");
			}
		}
	}
	fprintf(file, "\n};\n\n");

	fputs("enum cursor_type {\n", file);

	for (i = 0; i < n; ++i) {
		fprintf(file, "\tcursor_%s,\n", glyphs[i].name);
	}

	fputs("};\n\n", file);

	fprintf(file,
	        "static struct cursor {\n"
	        "\tint width, height;\n"
	        "\tint hotspot_x, hotspot_y;\n"
	        "\tsize_t offset;\n"
	        "} cursor_metadata[] = {\n");

	for (i = 0; i < n; ++i) {
		fprintf(file, "\t{ %d, %d, %d, %d, %zu }, /* %s */\n", glyphs[i].width,
		        glyphs[i].height, glyphs[i].hotspot_x, glyphs[i].hotspot_y,
		        glyphs[i].offset, glyphs[i].name);
	}

	fputs("};\n", file);
}

struct glyph *
find_mask_glyph(char *name)
{
	const char mask[] = "_mask";
	const int masklen = strlen(mask);

	int len = strlen(name);
	int i;
	for (i = 0; i < extracted_font.count; ++i) {
		struct glyph *g = &extracted_font.glyphs[i];
		int l2 = strlen(g->name);
		if ((l2 == len + masklen) && (memcmp(g->name, name, len) == 0) &&
		    (memcmp(g->name + len, mask, masklen) == 0)) {
			return g;
		}
	}
	return NULL;
}

static void
find_cursor_and_mask(const char *name,
                     struct glyph **cursor,
                     struct glyph **mask)
{
	int i;
	char mask_name[100];
	sprintf(mask_name, "%s_mask", name);

	*cursor = *mask = NULL;

	for (i = 0; i < extracted_font.count && (!*mask || !*cursor); ++i) {
		struct glyph *g = &extracted_font.glyphs[i];
		if (!strcmp(name, g->name)) {
			*cursor = g;
		} else if (!strcmp(mask_name, g->name)) {
			*mask = g;
		}
	}
}

static struct {
	char *target_name, *source_name;
} interesting_cursors[] = {{"bottom_left_corner", "bottom_left_corner"},
                           {"bottom_right_corner", "bottom_right_corner"},
                           {"bottom_side", "bottom_side"},
                           {"grabbing", "fleur"},
                           {"left_ptr", "left_ptr"},
                           {"left_side", "left_side"},
                           {"right_side", "right_side"},
                           {"top_left_corner", "top_left_corner"},
                           {"top_right_corner", "top_right_corner"},
                           {"top_side", "top_side"},
                           {"xterm", "xterm"},
                           {"hand1", "hand1"},
                           {"watch", "watch"}};

static void
output_interesting_cursors(FILE *file)
{
	int i;
	int n = sizeof(interesting_cursors) / sizeof(interesting_cursors[0]);
	struct reconstructed_glyph *glyphs = malloc(n * sizeof(*glyphs));

	for (i = 0; i < n; ++i) {
		struct glyph *cursor, *mask;
		find_cursor_and_mask(interesting_cursors[i].source_name, &cursor,
		                     &mask);
		if (!cursor) {
#if ENABLE_DEBUG
			fprintf(stderr, "no cursor for %s\n",
			        interesting_cursors[i].source_name);
#endif
			abort();
		}
		if (!mask) {
#if ENABLE_DEBUG
			fprintf(stderr, "no mask for %s\n",
			        interesting_cursors[i].source_name);
#endif
			abort();
		}
		reconstruct_glyph(cursor, mask, interesting_cursors[i].target_name,
		                  &glyphs[i]);
	}

	write_output_file(file, glyphs, n);
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
#if ENABLE_DEBUG
		fprintf(stderr, "Usage: %s input.pcf output.h\n", argv[0]);
#endif
		return EXIT_FAILURE;
	}

	int fd = open(argv[1], O_RDONLY);
	struct stat filestat;

	fstat(fd, &filestat);

	void *fontbuf = mmap(NULL, filestat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	handle_pcf(fontbuf);

	init_data_buffer();

	FILE *file = fopen(argv[2], "w");
	output_interesting_cursors(file);
	fclose(file);

	return EXIT_SUCCESS;
}
