#include "filesystem.hh"
#include "font.hh"
#include "st7789.h"

// FreeType
#include <freetype/ftoutln.h>
#include <freetype/internal/ftobjs.h>

// Harfbuzz
#include <hb.h>
#include <hb-ft.h>

// C++
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// C
#include "string.h"


// Embedded font resources (see add_resource in CMakeLists.txt)
extern "C" {
    extern uint8_t _binary_assets_OpenSans_Regular_Stripped_ttf_start[];
    extern uint8_t _binary_assets_OpenSans_Regular_Stripped_ttf_end[];

    const uint8_t* opensans_ttf = _binary_assets_OpenSans_Regular_Stripped_ttf_start;
    const uint8_t* opensans_ttf_end = _binary_assets_OpenSans_Regular_Stripped_ttf_end;
    const size_t opensans_ttf_size = opensans_ttf_end - opensans_ttf;
}

extern "C" {
    extern uint8_t _binary_assets_NotoSansMono_Regular_Stripped_otf_start[];
    extern uint8_t _binary_assets_NotoSansMono_Regular_Stripped_otf_end[];

    const uint8_t* notomono_otf = _binary_assets_NotoSansMono_Regular_Stripped_otf_start;
    const uint8_t* notomono_otf_end = _binary_assets_NotoSansMono_Regular_Stripped_otf_end;
    const size_t notomono_otf_size = notomono_otf_end - notomono_otf;
}

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


FontStore::FontStore()
    : m_face(nullptr),
      m_active_id(-1)
{
    FT_Error error = FT_Init_FreeType(&m_ft_library);
    if (error) {
        printf("FATAL (%s): FT_Init_FreeType error: 0x%02X\n", __func__, error);
        abort();
    }
}

FontStore::~FontStore()
{
    FT_Error error = FT_Done_FreeType(m_ft_library);
    if (error) {
        printf("FATAL (%s): FT_Done_FreeType error: 0x%02X\n", __func__, error);
        abort();
    }
}

void noop_destroy_func(void *user_data)
{
    // Do nothing
}

bool FontStore::drawGlyph(uint32_t codepoint, int adjust_y)
{
    uint32_t id = m_indexer.find(codepoint);

    if (id == 0xFFFF) {
        // No glyph available
        return false;
    }

    if (!loadFace(id)) {
        printf("Failed to load face %d\n", id);
        return false;
    }

    // {
    //     hb_buffer_t *buf;
    //     buf = hb_buffer_create();
    //     // hb_buffer_add_utf8(buf, "\U0001f3c3\u200d\U0001f3fe", -1, 0, -1);
    //     // hb_buffer_add_utf8(buf, "\u2764\uFE0F\u200D\U0001F525", -1, 0, -1);
    //     hb_buffer_add_utf8(buf, "\U0001F3F4\u200D\u2620\uFE0F", -1, 0, -1);

    //     hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    //     hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
    //     hb_buffer_set_language(buf, hb_language_get_default());

    //     hb_font_t *font = hb_ft_font_create(m_face, noop_destroy_func);

    //     hb_shape(font, buf, NULL, 0);

    //     unsigned int glyph_count;
    //     hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(buf, &glyph_count);
    //     hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    //     hb_position_t cursor_x = 0;
    //     hb_position_t cursor_y = 0;
    //     for (unsigned int i = 0; i < glyph_count; i++) {
    //         hb_codepoint_t glyphid  = glyph_info[i].codepoint;
    //         hb_position_t x_offset  = glyph_pos[i].x_offset;
    //         hb_position_t y_offset  = glyph_pos[i].y_offset;
    //         hb_position_t x_advance = glyph_pos[i].x_advance;
    //         hb_position_t y_advance = glyph_pos[i].y_advance;

    //         /* draw_glyph(glyphid, cursor_x + x_offset, cursor_y + y_offset); */

    //         // printf(" glyph: %d\n", glyphid);

    //         cursor_x += x_advance;
    //         cursor_y += y_advance;
    //     }

    //     hb_buffer_destroy(buf);
    //     hb_font_destroy(font);
    // }

    FT_Error error;
    int width = 0;
    int height = 0;

    const auto &slot = m_face->glyph;

    if (m_face->num_fixed_sizes > 0) {
        // Bitmap font: look for the most appropriate size available

        const int target_size_px = 128;

        uint best_index = 0;
        uint best_delta = 0xFFFF;

        for (uint i = 0; i < m_face->num_fixed_sizes; i++) {
            const uint delta = std::abs(target_size_px - m_face->available_sizes[i].height);
            if (delta < best_delta) {
                best_index = i;
                best_delta = delta;
            }
        }

        FT_Select_Size(m_face, best_index);
        error = FT_Load_Char(m_face, codepoint, FT_LOAD_DEFAULT | FT_LOAD_COLOR);

        if (error) {
            return false;
        }

    } else {
        // Load an outline glyph so that it will fit on screen

        const uint16_t max_width = DISPLAY_WIDTH - 20;
        const uint16_t max_height = DISPLAY_HEIGHT - 60;

        // Start with a size that will allow 95% of glyphs fit comfortably on screen
        uint point_size = 60;

        while (point_size != 0) {
            FT_Set_Char_Size(
                  m_face,
                  0, point_size * 64, // width and height in 1/64th of points
                  218, 218 // Device resolution
            );

            error = FT_Load_Char(m_face, codepoint, FT_LOAD_DEFAULT | FT_LOAD_COMPUTE_METRICS);

            // Get dimensions, rouded up
            // Since we're using COMPUTE_METRICS, this should be correct regardless of the font contents
            width = (slot->metrics.width + 32) / 64;
            height = (slot->metrics.height + 32) / 64;

            if (error || width == 0 || height == 0) {
                return false;
            }

            // Reduce font size proportionally if the glpyh won't fit on screen
            // Worst case we should only need two passes to find a fitting size
            if (width > max_width) {
                const uint new_size = (((max_width << 8) / width) * point_size) >> 8;

                if (new_size == point_size) {
                    point_size--;
                } else {
                    point_size = new_size;
                }

            } else if (height > max_height) {
                const uint new_size = (((max_height << 8) / height) * point_size) >> 8;

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
        offset.y = DISPLAY_HEIGHT - (((DISPLAY_HEIGHT - height)/2)) - offsetY + adjust_y;

        FT_Raster_Params params;
        memset(&params, 0, sizeof(params));
        params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
        params.gray_spans = raster_callback_mono_direct;
        params.user = &offset;

        // Blank out the previous drawing at the very last moment
        clearLastGlyph();

        FT_Outline_Render(m_ft_library, &slot->outline, &params);

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
        clearLastGlyph();

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

void FontStore::clearLastGlyph()
{
    if (m_last_draw.is_valid()) {
        st7789_fill_window(0x0, m_last_draw.x, m_last_draw.y, m_last_draw.width, m_last_draw.height);
        m_last_draw.invalidate();
    }
}

bool FontStore::loadFace(uint id)
{
    if (id == m_active_id) {
        // Already loaded: no action required
        return true;
    }

    unloadFace();

    if (id > m_font_table.size()) {
        printf("Error: request to load out of bounds font: id %d\n", id);
        return false;
    }

    const char* path = m_font_table.at(id).c_str();

    FT_Error error = fs::load_face(path, m_ft_library, &m_face);
    if (error) {
        printf("Error loading '%s': FreeType error 0x%02X\n", path, error);
        m_face = nullptr;
        return false;
    }

    m_active_id = id;

    return true;
}

void FontStore::unloadFace()
{
    if (m_face != nullptr) {
        FT_Done_Face(m_face);
        printf("Unloaded face %d\n", m_active_id);
        m_face = nullptr;
        m_active_id = -1;
    }
}

FT_Error FontStore::registerFont(const char* path)
{
    const uint id = m_font_table.size();

    if (id > 255) {
        printf("All font slots are taken! Refusing to register %s", path);
        return FT_Err_Out_Of_Memory;
    }

    FT_Face face;
    FT_Error error = fs::load_face(path, m_ft_library, &face);
    if (error) {
        printf("Error loading '%s': FreeType error 0x%02X\n", path, error);
        return error;
    }

    m_indexer.indexFace(id, face);

    FT_Done_Face(face);

    // Register the font only if it actually contributed codepoints
    // There's a lot of overlap in the Noto font set, so quite a few fonts end up unused.
    // (IDs are 8-bit to minimise the index's memory footprint)
    for (const auto &range : m_indexer.ranges()) {
        if (range.id == id) {
            m_font_table.emplace_back(path);
            break;
        }
    }

    return FT_Err_Ok;
}

UIFontPen FontStore::get_pen()
{
    return UIFontPen(opensans_ttf, opensans_ttf_size, m_ft_library);
}

UIFontPen FontStore::get_monospace_pen()
{
    return UIFontPen(notomono_otf, notomono_otf_size, m_ft_library);
}

//
// Pen
//

struct PenRasterState {
    int16_t buf_x;
    int16_t baseline;
    int16_t screen_x;
    int16_t screen_y;
    uint32_t colour;
    int16_t width;
    int16_t height;
    uint8_t* buffer;
};

/**
 * Raster spans to a line buffer in memory
 */
static void raster_pen_line(
    const PenRasterState* state,
    uint8_t* line_buf,
    const uint8_t* line_buf_end,
    const int count,
    const FT_Span* const spans
)
{
    const uint8_t pen_r = state->colour >> 16;
    const uint8_t pen_g = state->colour >> 8;
    const uint8_t pen_b = state->colour;

    for (int i = 0; i < count; ++i) {
        const auto &span = spans[i];

        // Fade the colour based on coverage
        // This probably isn't correct, but it's fast
        const uint8_t r = (pen_r * span.coverage) >> 8;
        const uint8_t g = (pen_g * span.coverage) >> 8;
        const uint8_t b = (pen_b * span.coverage) >> 8;

        int16_t x = (state->buf_x + span.x);
        const int16_t end_x = x + span.len;

        uint8_t* local_buf = x < 0 ? line_buf : line_buf + (x * 3);

        while (x < end_x && x < state->width) {
            if (x > 0) {
                if (local_buf >= line_buf_end) {
                    break;
                }

                *(local_buf++) = r;
                *(local_buf++) = g;
                *(local_buf++) = b;
            }

            x++;
        }
    }
}

static void raster_callback_canvas(const int y, const int count, const FT_Span* const spans, void * const user)
{
    const PenRasterState* state = (const PenRasterState*) user;
    const int canvas_y = state->height - y + state->baseline;

    if (canvas_y < 0 || canvas_y >= (state->height - 1)) {
        return;
    }

    uint8_t* line_buf = state->buffer + (canvas_y * state->width * 3);
    const uint8_t* buf_end = state->buffer + (state->height * state->width * 3) - 1;

    raster_pen_line(state, line_buf, buf_end, count, spans);
}

static void raster_callback_line(const int y, const int count, const FT_Span* const spans, void * const user)
{
    const PenRasterState* state = (const PenRasterState*) user;
    const int canvas_y = state->height - y + state->baseline;

    if (canvas_y < 0 || canvas_y >= (state->height - 1)) {
        return;
    }

    uint8_t* buf = st7789_line_buffer();
    const uint8_t* buf_end = buf + ST7789_LINE_BUF_SIZE - 1;
    memset(buf, 0, ST7789_LINE_BUF_SIZE);

    const int16_t line_start = spans[0].x;
    const int16_t line_end = std::min(
        ST7789_LINE_LEN_PX,
        spans[count-1].x + spans[count-1].len
    );

    raster_pen_line(state, buf, buf_end, count, spans);

    const uint16_t start_x = state->buf_x;
    const uint16_t end_x = std::min(DISPLAY_WIDTH, start_x + spans[count -1].x + spans[count -1].len);

    st7789_set_cursor(state->screen_x + state->buf_x, state->screen_y + state->height - canvas_y);
    st7789_write_dma(buf + (start_x * 3), (end_x - start_x) * 3, true);
}

static void raster_callback_direct(const int y, const int count, const FT_Span* const spans, void * const user)
{
    const PenRasterState* state = (const PenRasterState*) user;
    const int canvas_y = state->height - y + state->baseline;

    const uint8_t pen_r = state->colour >> 16;
    const uint8_t pen_g = state->colour >> 8;
    const uint8_t pen_b = state->colour;

    for (int i = 0; i < count; ++i) {
        const auto &span = spans[i];

        // Fade the colour based on coverage
        // This probably isn't correct, but it's fast
        const uint8_t channels[3] = {
            (uint8_t) ((pen_r * span.coverage) >> 8),
            (uint8_t) ((pen_g * span.coverage) >> 8),
            (uint8_t) ((pen_b * span.coverage) >> 8),
        };

        st7789_set_cursor(state->screen_x + state->buf_x + span.x, state->screen_y + canvas_y);

        for (uint k = 0; k < span.len; k++) {
            st7789_write((uint8_t*) &channels, sizeof(channels));
        }
    }
}

UIFontPen::UIFontPen(const uint8_t* fontdata, size_t length, FT_Library library)
    : m_ft_library(library),
      m_face(nullptr),
      m_error(0),
      m_x(0),
      m_y(0),
      m_colour(0xFFFFFF),
      m_size_px(16),
      m_embolden(0),
      m_mode(UIFontPen::kMode_CanvasBuffer)
{
    m_error = FT_New_Memory_Face(library, fontdata, length, 0, &m_face);
    if (m_error) {
        printf("Error: Embedded font load Failed: 0x%02X\n", m_error);
    }
}

UIFontPen::~UIFontPen()
{
    FT_Done_Face(m_face);
}

void UIFontPen::set_size(uint16_t size_px)
{
    m_size_px = size_px;
    FT_Set_Pixel_Sizes(m_face, 0, size_px);
}

uint16_t UIFontPen::compute_px_width(const char* str)
{
    uint16_t px_width = 0;

    {
        uint16_t index = 0;
        while (str[index] != '\0') {
            FT_Load_Char(m_face, str[index], FT_LOAD_DEFAULT | FT_LOAD_BITMAP_METRICS_ONLY);
            px_width += m_face->glyph->advance.x / 64;
            index++;
        }
    }

    return px_width;
}

UIRect UIFontPen::draw(const char* str)
{
    // Measure how width our buffer needs to be, since it's not pre-calculated
    return draw(str, compute_px_width(str));
}

UIRect UIFontPen::draw(const char* str, const uint16_t canvas_width_px)
{
    if (m_error) {
        printf("Unable to draw as the face is in an error state\n");
        return UIRect();
    }

    if (canvas_width_px == 0 || str == NULL || *str == '\0') {
        // Nothing to draw
        return UIRect();
    }

    // Constrain canvas to available dimensions at pen position
    // The height is cheated a bit here, as it doesn't account for the
    // font's baseline, meaning we lose the bottom of some characters
    const int16_t px_width = std::min((int16_t)(DISPLAY_WIDTH - m_x + 1), (int16_t)canvas_width_px);
    const int16_t px_height = m_size_px + (m_embolden/64) - (m_face->descender/64);
    // const int16_t px_height = (m_face->height/64) + (m_embolden/64);

    const uint16_t canvas_bytes = px_width * px_height * 3;


    PenRasterState state;
    state.buf_x = 0;
    state.baseline = (m_face->descender/64);
    state.screen_x = m_x;
    state.screen_y = m_y;
    state.colour = m_colour;
    state.width = px_width;
    state.height = px_height;

    FT_Raster_Params params;
    memset(&params, 0, sizeof(params));
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    params.user = &state;

    if (m_mode == UIFontPen::kMode_CanvasBuffer) {
        state.buffer = (uint8_t*) calloc(1, canvas_bytes);
        if (state.buffer == NULL) {
            printf("Failed to allocate render buffer of %d x %d\n", px_width, px_height);
            return UIRect();
        }

        params.gray_spans = raster_callback_canvas;

    } else if (m_mode == UIFontPen::kMode_LineBuffer) {
        state.buffer = nullptr;
        params.gray_spans = raster_callback_line;

    } else if (m_mode == UIFontPen::kMode_DirectToScreen) {
        state.buffer = nullptr;
        params.gray_spans = raster_callback_direct;

    } else {
        printf("No such render mode: %d\n", m_mode);
        return UIRect();
    }

    uint16_t index = 0;
    while (str[index] != '\0') {
        if (state.buf_x >= DISPLAY_WIDTH - 1) {
            break;
        }

        FT_Load_Char(m_face, str[index], FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);

        const auto &slot = m_face->glyph;

        if (slot->format == FT_GLYPH_FORMAT_OUTLINE) {
            if (m_embolden != 0) {
                FT_Outline_Embolden(&slot->outline, m_embolden);
            }

            FT_Outline_Render(m_ft_library, &slot->outline, &params);
        }

        state.buf_x += slot->advance.x / 64;
        index++;
    }

    // Send rendered line to screen if needed
    if (m_mode == UIFontPen::kMode_CanvasBuffer) {
        st7789_set_window(m_x, m_y, m_x + px_width, m_y + px_height);
        st7789_write_dma(state.buffer, px_width * px_height * 3, true);

        // Ensure writing completes before we deallocate the buffer
        st7789_deselect();

        free(state.buffer);
    }

    // Absorb the total pen movement into our state
    m_x += state.buf_x;

    return UIRect(state.screen_x + 1, state.screen_y, state.width, state.height + 1);
}

void UIRect::clamp(uint16_t min_x, uint16_t min_y, uint16_t max_x, uint16_t max_y)
{
    const uint16_t x2 = std::min(static_cast<uint16_t>(x + width), max_x);
    const uint16_t y2 = std::min(static_cast<uint16_t>(y + height), max_y);

    x = std::max(x, min_x);
    y = std::max(y, min_y);

    width = x2 - x;
    height = y2 - y;
}

void UIRect::merge(const UIRect &other)
{
    const int16_t x2 = std::max(x + width, other.x + other.width);
    const int16_t y2 = std::max(y + height, other.y + other.height);

    x = std::min(x, other.x);
    y = std::min(y, other.y);
    width = x2 - x;
    height = y2 - y;
}

UIRect& UIRect::operator+=(const UIRect& other)
{
    merge(other);
    return *this;
}

void UIRect::draw_outline_debug(uint32_t colour) const
{
    if (!is_valid()) {
        return;
    }

    for (uint16_t draw_y = y; draw_y < (y + height - 1); draw_y++) {
        st7789_set_cursor(x, draw_y);
        st7789_put(colour);

        st7789_set_cursor(x + width - 1, draw_y);
        st7789_put(colour);
    }

    for (uint16_t draw_x = x; draw_x < (x + width - 1); draw_x++) {
        st7789_set_cursor(draw_x, y);
        st7789_put(colour);

        st7789_set_cursor(draw_x, y + height - 1);
        st7789_put(colour);
    }
}