#include "svg.hh"

#include <lunasvg.h>

#ifndef FT_CONFIG_OPTION_SVG
    #error "SVG support is disabled in this build of FreeType"
#endif

FT_Error lunasvg_port_init(FT_Pointer *state)
{
    printf("lunasvg_port_init\n");
    return FT_Err_Ok;
}

void lunasvg_port_free(FT_Pointer *state)
{
    printf("lunasvg_port_free\n");
}

FT_Error lunasvg_port_render(FT_GlyphSlot slot, FT_Pointer *state)
{
    // Note these are pixel sizes, not points
    const int width = slot->bitmap.width;
    const int height = slot->bitmap.rows;
    const int stride = slot->bitmap.pitch;

    printf("Size: %d x %d x %d\n", width, height);
    printf("Buffer: %lu\n", slot->bitmap.buffer);

    // auto document = lunasvg::Document::loadFromData();
    // auto bitmap = document->renderToBitmap(width, height);

    // const double rootWidth = document->width();
    // const double rootHeight = document->height();

    // // LunaSVG uses ARGB, so this is probably wrong for FreeType
    // lunasvg::Bitmap bitmap((uint8_t*) slot->bitmap.buffer, width, height, stride);
    // lunasvg::Matrix matrix(width / rootWidth, 0, 0, height / rootHeight, 0, 0);

    // bitmap.clear(0);
    // document->render(bitmap, matrix);

    // slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    // slot->bitmap.num_grays = 256;
    // slot->format = FT_GLYPH_FORMAT_BITMAP;

    printf("lunasvg_port_render\n");

    return FT_Err_Ok;
}

FT_Error lunasvg_port_preset_slot(FT_GlyphSlot slot, FT_Bool cache, FT_Pointer *state)
{
    // Example: https://gitlab.freedesktop.org/freetype/freetype-demos/-/blob/master/src/rsvg-port.c
    // API docs: https://freetype.org/freetype2/docs/reference/ft2-svg_fonts.html

    FT_SVG_Document document = (FT_SVG_Document) slot->other;
    FT_Size_Metrics metrics = document->metrics;

    // document->svg_document;
    // document->svg_document_length;

    printf("lunasvg_port_preset_slot with SVG size %lu\n", document->svg_document_length);

    return FT_Err_Ok;
}

SVG_RendererHooks lunasvg_hooks = {
    (SVG_Lib_Init_Func) lunasvg_port_init,
    (SVG_Lib_Free_Func) lunasvg_port_free,
    (SVG_Lib_Render_Func) lunasvg_port_render,
    (SVG_Lib_Preset_Slot_Func) lunasvg_port_preset_slot
};