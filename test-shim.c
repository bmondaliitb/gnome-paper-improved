#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

static unsigned long
coverage(FT_Face face, FT_Int32 flags, const char *weight)
{
    assert(setenv("PDF_FONT_WEIGHT", weight, 1) == 0);
    assert(FT_Load_Glyph(face, FT_Get_Char_Index(face, 'H'), flags) == 0);
    if (!(flags & FT_LOAD_RENDER))
        assert(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0);
    assert(face->glyph->format == FT_GLYPH_FORMAT_BITMAP);
    FT_Bitmap *bitmap = &face->glyph->bitmap;
    assert(bitmap->pixel_mode == FT_PIXEL_MODE_GRAY);
    unsigned long total = 0;
    for (unsigned y = 0; y < bitmap->rows; ++y)
        for (unsigned x = 0; x < bitmap->width; ++x)
            total += bitmap->buffer[y * bitmap->pitch + x];
    return total;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FT_Library library;
    FT_Face face;
    assert(FT_Init_FreeType(&library) == 0);
    assert(FT_New_Face(library, argv[1], 0, &face) == 0);
    assert(FT_Set_Pixel_Sizes(face, 0, 24) == 0);
    FT_Int32 paths[] = { FT_LOAD_DEFAULT, FT_LOAD_RENDER };
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        unsigned long normal = coverage(face, paths[i], "0");
        unsigned long bold = coverage(face, paths[i], "64");
        printf("flags=%ld: normal=%lu bold=%lu\n", (long)paths[i], normal, bold);
        assert(bold > normal);
        assert(coverage(face, paths[i], "0") == normal);
    }
    assert(FT_Load_Glyph(face, FT_Get_Char_Index(face, 'H'),
                         FT_LOAD_RENDER | FT_LOAD_MONOCHROME) == 0);
    assert(face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO);
    FT_BBox normal_box, bold_box;
    assert(setenv("PDF_FONT_WEIGHT", "0", 1) == 0);
    assert(FT_Load_Glyph(face, FT_Get_Char_Index(face, 'H'), FT_LOAD_NO_SCALE) == 0);
    FT_Outline_Get_CBox(&face->glyph->outline, &normal_box);
    assert(setenv("PDF_FONT_WEIGHT", "64", 1) == 0);
    assert(FT_Load_Glyph(face, FT_Get_Char_Index(face, 'H'), FT_LOAD_NO_SCALE) == 0);
    FT_Outline_Get_CBox(&face->glyph->outline, &bold_box);
    assert(normal_box.xMin == bold_box.xMin && normal_box.xMax == bold_box.xMax);
    assert(normal_box.yMin == bold_box.yMin && normal_box.yMax == bold_box.yMax);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    puts("PASS: both render paths darken, zero resets, mono and unscaled loads preserved.");
    return 0;
}
