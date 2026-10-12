/*
 * ft-pdf-bold.c
 *
 * Small LD_PRELOAD shim for FreeType that slightly emboldens outline glyphs
 * after FT_Load_Glyph() has loaded them.
 *
 * Intended to be used only for Papers via a wrapper such as:
 *
 *   PDF_FONT_WEIGHT=24 \
 *   LD_PRELOAD="$HOME/.local/lib/papers-bold/ft-pdf-bold.so" \
 *   /usr/bin/papers
 *
 * PDF_FONT_WEIGHT is expressed in FreeType 26.6 coordinate units:
 *   16 = 0.25 px
 *   24 = 0.375 px
 *   32 = 0.50 px
 *   48 = 0.75 px
 *   64 = 1.00 px
 *
 * Build:
 *
 *   gcc -shared -fPIC \
 *       $(pkg-config --cflags freetype2) \
 *       ft-pdf-bold.c \
 *       -o ft-pdf-bold.so \
 *       -ldl -lfreetype -pthread
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

typedef FT_Error (*ft_load_glyph_fn)(FT_Face face,
                                     FT_UInt glyph_index,
                                     FT_Int32 load_flags);

static ft_load_glyph_fn real_FT_Load_Glyph = NULL;
static pthread_once_t resolve_once = PTHREAD_ONCE_INIT;
static atomic_ulong loaded_glyphs;
static atomic_ulong emboldened_glyphs;
static int debug_enabled;

__attribute__((constructor)) static void
shim_loaded(void)
{
    const char *debug = getenv("PDF_FONT_DEBUG");
    debug_enabled = debug != NULL && *debug != '\0' && *debug != '0';
    if (debug_enabled)
        fprintf(stderr, "papers-bold: shim loaded (pid=%ld)\n", (long)getpid());
}

__attribute__((destructor)) static void
shim_unloaded(void)
{
    if (debug_enabled)
        fprintf(stderr, "papers-bold: pid=%ld loaded=%lu emboldened=%lu\n",
                (long)getpid(), atomic_load(&loaded_glyphs),
                atomic_load(&emboldened_glyphs));
}

/*
 * Resolve the original FreeType function from the next object in the dynamic
 * linker's lookup order. Resolve once, safely across rendering threads.
 */
static void
resolve_FT_Load_Glyph(void)
{
    real_FT_Load_Glyph =
        (ft_load_glyph_fn)dlsym(RTLD_NEXT, "FT_Load_Glyph");
}

/*
 * Return the requested emboldening strength.
 *
 * Keep the default modest.  Negative/zero values disable emboldening.
 * The upper clamp prevents an accidental environment value from producing
 * absurd outlines.
 */
static FT_Pos
get_embolden_strength(void)
{
    const char *value = getenv("PDF_FONT_WEIGHT");

    if (value == NULL || *value == '\0')
        return 24;

    errno = 0;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0')
        return 24;

    if (parsed <= 0)
        return 0;

    if (parsed > 256)
        parsed = 256;

    return (FT_Pos)parsed;
}

FT_Error
FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags)
{
    pthread_once(&resolve_once, resolve_FT_Load_Glyph);
    ft_load_glyph_fn real_load = real_FT_Load_Glyph;

    /*
     * If the real symbol cannot be found, return a FreeType error rather than
     * dereferencing a NULL function pointer.
     */
    if (real_load == NULL)
        return FT_Err_Invalid_Argument;

    FT_Pos strength = get_embolden_strength();
    if (debug_enabled && atomic_fetch_add(&loaded_glyphs, 1) == 0)
        fprintf(stderr, "papers-bold: glyph hook reached (weight=%ld flags=0x%lx)\n",
                (long)strength, (unsigned long)load_flags);

    /* Unscaled outlines use font units, not the pixel units of our setting.
     * Preserve FreeType's semantics for metrics-only and composite loads. */
    if (strength == 0 || face == NULL || face->size == NULL ||
        face->size->metrics.x_ppem == 0 || face->size->metrics.y_ppem == 0 ||
        (load_flags & (FT_LOAD_NO_SCALE | FT_LOAD_NO_RECURSE |
                       FT_LOAD_BITMAP_METRICS_ONLY)))
        return real_load(face, glyph_index, load_flags);

    /* Embolden BEFORE rasterization, even when the caller requests a bitmap
     * directly through FT_LOAD_RENDER. Previously those loads were skipped. */
    int render_requested = (load_flags & FT_LOAD_RENDER) != 0;
    FT_Error error = real_load(face, glyph_index, load_flags & ~FT_LOAD_RENDER);

    if (error != FT_Err_Ok || face == NULL || face->glyph == NULL)
        return error;

    FT_GlyphSlot slot = face->glyph;

    /*
     * FT_Outline_EmboldenXY() is valid only for outline glyphs.  Bitmap,
     * SVG, and other glyph formats are left untouched.
     *
     * Ignore emboldening failure deliberately: loading the glyph succeeded,
     * so rendering should continue with the original outline rather than
     * turning a cosmetic enhancement into a PDF rendering failure.
     */
    if (strength > 0 &&
        slot->format == FT_GLYPH_FORMAT_OUTLINE &&
        slot->outline.n_points > 0) {
        FT_Error bold_error = FT_Outline_EmboldenXY(&slot->outline, strength, strength);
        if (bold_error == FT_Err_Ok && debug_enabled &&
            atomic_fetch_add(&emboldened_glyphs, 1) == 0)
            fprintf(stderr, "papers-bold: outline emboldened (weight=%ld)\n",
                    (long)strength);
    }

    if (render_requested && slot->format != FT_GLYPH_FORMAT_BITMAP) {
        FT_Render_Mode mode = FT_LOAD_TARGET_MODE(load_flags);
        if (mode == FT_RENDER_MODE_NORMAL && (load_flags & FT_LOAD_MONOCHROME))
            mode = FT_RENDER_MODE_MONO;
        return FT_Render_Glyph(slot, mode);
    }

    return error;
}
