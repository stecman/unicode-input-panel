#pragma once

#include <ft2build.h>
#include <freetype/otsvg.h>
#include <freetype/freetype.h>

FT_Error lunasvg_port_init(FT_Pointer *state);
void lunasvg_port_free(FT_Pointer *state);
FT_Error lunasvg_port_render(FT_GlyphSlot slot, FT_Pointer *state);
FT_Error lunasvg_port_preset_slot(FT_GlyphSlot slot, FT_Bool cache, FT_Pointer *state);

extern SVG_RendererHooks lunasvg_hooks;