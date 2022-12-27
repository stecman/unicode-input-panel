#include "glyph_display.hh"

#include "unicode_db.hh"
#include "st7789.h"

// FreeType
#include <freetype/ftoutln.h>
#include <freetype/internal/ftobjs.h>

/**
 * Render spans directly to screen, using DMA to send longer lengths of pixels
 */
static void raster_callback_mono_direct(const int y, const int count, const FT_Span* const spans, void * const user)
{
    static uint8_t px_value[2];
    static uint8_t active_index = 0;

    FT_Vector* offset = (FT_Vector*) user;

    for (int i = 0; i < count; ++i) {
        const auto &span = spans[i];

        st7789_set_cursor(offset->x + span.x, offset->y - y);

        if (span.len == 1) {
            // For tiny spans, skip DMA as it's faster to send directly
            st7789_put_mono(span.coverage);

        } else {

            // Alternate value buffers to avoid disturbing in-progress DMA
            active_index = !active_index;
            px_value[active_index] = span.coverage;

            st7789_write_dma(&px_value[active_index], span.len * 3, false);
        }
    }
}

static void raster_callback_mono_line(const int y, const int count, const FT_Span* const spans, void * const user)
{
    FT_Vector* offset = (FT_Vector*) user;
    uint8_t* buf = st7789_line_buffer();
    memset(buf, 0, ST7789_LINE_BUF_SIZE);

    for (uint i = 0; i < count; i++) {
        const auto &span = spans[i];

        uint8_t* local_buf = buf + span.x * 3;
        const uint8_t value = span.coverage;

        for (uint k = 0; k < span.len; k++) {
            *(local_buf++) = value;
            *(local_buf++) = value;
            *(local_buf++) = value;
        }
    }

    const uint16_t min_x = spans[0].x;
    const uint16_t max_x = std::min(ST7789_LINE_LEN_PX, spans[count - 1].x + spans[count - 1].len);
    const uint16_t length = max_x - min_x;

    st7789_set_cursor(offset->x + min_x, offset->y - y);
    st7789_write_dma(buf + (min_x * 3), length * 3, true);
}


GlyphDisplay::GlyphDisplay(FontStore& fontstore, uint16_t max_width, uint16_t max_height, int y_offset)
    : m_y_offset(y_offset),
      m_max_width(max_width),
      m_max_height(max_height),
      m_last_result(kResult_None),
      m_fontstore(fontstore) {}

void GlyphDisplay::clear()
{
    m_last_result = kResult_None;
    m_last_draw.blank_and_invalidate();
    m_last_fallback_draw.blank_and_invalidate();
}

void GlyphDisplay::draw(uint32_t codepoint, bool is_valid)
{
    static const char* s_control_char = "CTRL CODE";
    static const char* s_missing_glyph = "NO GLYPH";

    if (is_control_char(codepoint)) {
        // Technically valid codepoint, but has no visual representation

        if (m_last_result == kResult_ControlChar) {
            // Already drew this last call
            return;
        }

        UIFontPen pen = m_fontstore.get_pen();
        pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

        pen.set_size(34);
        pen.set_embolden(128);
        pen.set_colour(kColour_Gray);

        clear();

        const uint16_t text_width = pen.compute_px_width(s_control_char);
        pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), ((DISPLAY_HEIGHT)/2) - 20);
        m_last_fallback_draw = pen.draw(s_control_char, text_width);

        m_last_result = kResult_ControlChar;

    } else {
        const bool didDrawGlyph = drawGlyph(codepoint);

        if (didDrawGlyph) {
            m_last_result = kResult_DrewGlyph;

        } else if (is_valid) {
            // Apparently a valid codepoint, but not in any font we have

            if (m_last_result == kResult_MissingGlyph) {
                // Already drew this last time
                return;
            }

            UIFontPen pen = m_fontstore.get_pen();
            pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

            pen.set_size(34);
            pen.set_embolden(128);
            pen.set_colour(kColour_Gray);

            const uint16_t text_width = pen.compute_px_width(s_missing_glyph);
            pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), ((DISPLAY_HEIGHT)/2) - 20);

            clear();
            m_last_fallback_draw = pen.draw(s_missing_glyph, text_width);
            m_last_result = kResult_MissingGlyph;

        } else {
            // No glyph and hinted as invalid, so treat as invalid

            UIFontPen pen = m_fontstore.get_pen();
            pen.set_render_mode(UIFontPen::kMode_DirectToScreen);

            char _hex_string[12];
                                                                                        char* hex_string = (char*) &_hex_string;
            sprintf(hex_string, "0x%X", codepoint);

            // Adjust font size to fit on screen
            int yOffset;
            if (0xFF000000 & codepoint) {
                pen.set_size(32);
                yOffset = 22;
            } else {
                pen.set_size(44);
                yOffset = 28;
            }

            pen.set_colour(kColour_Gray);
            pen.set_embolden(128);

            const uint16_t text_width = pen.compute_px_width(hex_string);
            pen.move_to(std::max(0, (DISPLAY_WIDTH - text_width)/2), (DISPLAY_HEIGHT/2) - yOffset);

            clear();
            m_last_fallback_draw = pen.draw(hex_string, text_width);
            m_last_result = kResult_InvalidCodepoint;
        }
    }
}

bool GlyphDisplay::drawGlyph(uint32_t codepoint)
{
    FT_Face face = m_fontstore.loadFaceByCodepoint(codepoint);
    if (face == nullptr) {
        return false;
    }

    FT_Error error;
    int width = 0;
    int height = 0;

    const auto &slot = face->glyph;

    if (face->num_fixed_sizes > 0) {
        // Bitmap font: look for the most appropriate size available

        const int target_size_px = 128;

        uint best_index = 0;
        uint best_delta = 0xFFFF;

        for (uint i = 0; i < face->num_fixed_sizes; i++) {
            const uint delta = std::abs(target_size_px - face->available_sizes[i].height);
            if (delta < best_delta) {
                best_index = i;
                best_delta = delta;
            }
        }

        FT_Select_Size(face, best_index);
        error = FT_Load_Char(face, codepoint, FT_LOAD_DEFAULT | FT_LOAD_COLOR);

        if (error) {
            return false;
        }

    } else {
        // Load an outline glyph so that it will fit on screen

        // Start with a size that will allow 95% of glyphs fit comfortably on screen
        uint point_size = 60;

        while (point_size != 0) {
            FT_Set_Char_Size(
                  face,
                  0, point_size * 64, // width and height in 1/64th of points
                  218, 218 // Device resolution
            );

            // Load without auto-hinting, since hinting data isn't used with FT_Outline_Render
            // and auto-hinting can be memory intensive on complex glyphs. FT_LOAD_NO_HINTING
            // appears to make the font metrics inaccurate so I'm not using that here.
            const uint flags = FT_LOAD_DEFAULT | FT_LOAD_COMPUTE_METRICS | FT_LOAD_NO_AUTOHINT;
            error = FT_Load_Char(face, codepoint, flags);

            // Get dimensions, rouded up
            // Since we're using COMPUTE_METRICS, this should be correct regardless of the font contents
            width = (slot->metrics.width + 32) / 64;
            height = (slot->metrics.height + 32) / 64;

            if (error || width == 0 || height == 0) {
                return false;
            }

            // Reduce font size proportionally if the glpyh won't fit on screen
            // Worst case we should only need two passes to find a fitting size
            if (width > m_max_width) {
                const uint new_size = (((m_max_width << 8) / width) * point_size) >> 8;

                if (new_size == point_size) {
                    point_size--;
                } else {
                    point_size = new_size;
                }

            } else if (height > m_max_height) {
                const uint new_size = (((m_max_height << 8) / height) * point_size) >> 8;

                if (new_size == point_size) {
                    point_size--;
                } else {
                    point_size = new_size;
                }

            } else {
                // Glyph is an acceptable size
                break;
            }
        };
    }

    // Draw the glyph to screen
    if (slot->format == FT_GLYPH_FORMAT_OUTLINE) {

        // Calculation offsets to center the glyph on screen
        const int offsetY = ((slot->metrics.height - slot->metrics.horiBearingY) / 64 );
        const int offsetX = (slot->metrics.horiBearingX / 64);

        FT_Vector offset;
        offset.x = ((DISPLAY_WIDTH - width)/2) - offsetX;
        offset.y = DISPLAY_HEIGHT - (((DISPLAY_HEIGHT - height)/2)) - offsetY + m_y_offset;

        FT_Raster_Params params;
        memset(&params, 0, sizeof(params));
        params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
        params.gray_spans = raster_callback_mono_direct;
        params.user = &offset;

        // Blank out the previous drawing at the very last moment
        clear();

        FT_Outline_Render(m_fontstore.get_library(), &slot->outline, &params);

        // Store drawn region for blanking next glyph
        // This removes the compensation for baseline and bearing added to drew exactly centred
        m_last_draw.x = offset.x + offsetX;
        m_last_draw.y = offset.y + offsetY - height; // flip as glyph drawing is bottom-up
        m_last_draw.width = width;
        m_last_draw.height = height + 1;

    } else {

        // Use the built-in PNG rendering in FreeType
        //
        // TODO: This isn't ideal as it renders the entire image to a memory buffer.
        //       For Noto Emoji with 136 x 128 bitmaps, this uses about 70KB of memory.
        //
        //       Libpng supports rendering individual lines with a custom function, but
        //       FreeType doesn't expose the PNG image at all. This is all internal to
        //       FreeType, so it would probably need some linker trickery or a source
        //       patch to hook into the PNG scanline rendering API (png_read_row)
        //
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        // Blank out the previous drawing at the very last moment
        clear();

        width = slot->bitmap.width;
        height = slot->bitmap.rows;

        const uint16_t x = (DISPLAY_WIDTH - width)/2;
        const uint16_t y = (DISPLAY_HEIGHT - height)/2;

        st7789_set_window(x, y, x + width, y + height);

        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            uint8_t* src = slot->bitmap.buffer;

            for (uint y = 0; y < height; y++) {
                uint8_t* buf = st7789_line_buffer();
                uint8_t* ptr = buf;

                for (uint x = 0; x < width; x++) {
                    const uint8_t b = *(src++);
                    const uint8_t g = *(src++);
                    const uint8_t r = *(src++);
                    src++; // Ignore alpha channel

                    *(ptr++) = r;
                    *(ptr++) = g;
                    *(ptr++) = b;
                }

                st7789_write_dma(buf, width * 3, true);
            }

        } else {
            uint8_t* src = slot->bitmap.buffer;

            for (uint y = 0; y < height; y++) {
                uint8_t* buf = st7789_line_buffer();
                uint8_t* ptr = buf;

                for (uint x = 0; x < width; x++) {
                    const uint8_t v = *src++;

                    *(ptr++) = v;
                    *(ptr++) = v;
                    *(ptr++) = v;
                }

                st7789_write_dma(buf, width * 3, true);
            }
        }

        // Reclaim the memory used by the bitmap
        // (otherwise it remains resident until the next glyph load)
        ft_glyphslot_free_bitmap(slot);

        // Store drawn region for blanking next glyph
        m_last_draw.x = x;
        m_last_draw.y = y;
        m_last_draw.width = width;
        m_last_draw.height = height;
    }

    return true;
}