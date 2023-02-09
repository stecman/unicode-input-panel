#include "embeds.hh"
#include "filesystem.hh"
#include "font.hh"
#include "st7789.h"
#include "svg.hh"

// FreeType
#include <freetype/ftoutln.h>
#include <freetype/internal/ftobjs.h>
#include <freetype/internal/ftstream.h>
#include <freetype/tttables.h>
#include <freetype/tttags.h>

// C++
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// C
#include <ctype.h>
#include <string.h>


FontStore::FontStore()
    : m_face(nullptr),
      m_active_id(-1)
{
    FT_Error error = FT_Init_FreeType(&m_ft_library);
    if (error) {
        printf("FATAL (%s): FT_Init_FreeType error: 0x%02X\n", __func__, error);
        abort();
    }

    FT_Property_Set( m_ft_library, "ot-svg", "svg-hooks", &lunasvg_hooks );
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

FT_Face FontStore::loadFaceByCodepoint(uint32_t codepoint)
{
    uint32_t id = m_indexer.find(codepoint);

    if (id == 0xFFFF) {
        // No glyph available
        return nullptr;
    }

    return loadFace(id);
}

FT_Face FontStore::loadFace(uint32_t id)
{
    if (id == m_active_id) {
        // Already loaded: no action required
        return m_face;
    }

    UIFontPen::unload_shared();
    unloadFace();

    if (id > m_font_table.size()) {
        printf("Error: request to load out of bounds font: id %d\n", id);
        return nullptr;
    }

    const char* path = m_font_table.at(id).c_str();

    FT_Error error = fs::load_face(path, m_ft_library, &m_face);
    if (error) {
        printf("Error loading '%s': FreeType error 0x%02X\n", path, error);
        m_face = nullptr;
    } else {
        m_active_id = id;
    }

    return m_face;
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
    const uint32_t id = m_font_table.size();

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

void hexdump(void *ptr, int buflen) {
    unsigned char *buf = (unsigned char*)ptr;
    int i, j;

    for (i=0; i<buflen; i+=16) {
        printf("%06x: ", i);
        for (j=0; j<16; j++) { 
            if (i+j < buflen) {
                printf("%02x ", buf[i+j]);
            } else {
                printf("   ");
            }
        }
        printf(" ");
        for (j=0; j<16; j++) {
            if (i+j < buflen) {
                printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
            }
        }

        printf("\n");
    }
}

bool find_substitutions(FT_Face m_face)
{
    // Request the table length
    FT_ULong io_length = 0;
    FT_Error error = FT_Load_Sfnt_Table( m_face, TTAG_GSUB, 0, NULL, &io_length );
    if ( error ) {
        printf("No such table...\n");
        return false;
    }

    printf("  Want %lu bytes...\n", io_length);
    uint8_t* buffer = (uint8_t*) malloc(io_length);
    if ( buffer == NULL ) {
        printf("  Failed to malloc\n");
        return false;
    }

    error = FT_Load_Sfnt_Table( m_face, TTAG_GSUB, 0, buffer, &io_length );
    if ( error ) {
        printf("  Failed to load table\n");
        free(buffer);
        return false;
    }

    hexdump(buffer, 128);

    // Check this is a compatible GSUB table version
    uint16_t lookup_list_offset;
    {
        uint8_t* ptr = buffer;
        const uint16_t major_version = FT_NEXT_USHORT(ptr);
        const uint16_t minor_version = FT_NEXT_USHORT(ptr);

        lookup_list_offset = FT_NEXT_USHORT(ptr);

        if (major_version != 1) {
            printf("Unknown GSUB table version: %u.%u\n", major_version, minor_version);
            free(buffer);
            return false;
        }
    }

    // Iterate over lookup list entries
    {
        uint8_t* lookup_list_ptr = buffer + lookup_list_offset;

        const uint16_t lookup_count = FT_NEXT_USHORT(lookup_list_ptr);
        for (uint16_t i = 0; i < lookup_count; i++) {
            const uint16_t lookup_table_offset = FT_NEXT_USHORT(lookup_list_ptr);

            uint8_t* lookup_ptr = buffer + lookup_list_offset + lookup_table_offset;

            const uint16_t lookup_type = FT_NEXT_USHORT(lookup_ptr);
            const uint16_t lookup_flag = FT_NEXT_USHORT(lookup_ptr);
            const uint16_t lookup_subtable_count = FT_NEXT_USHORT(lookup_ptr);

            printf("  Table %u -> %u, %u, %u\n", i, lookup_type, lookup_flag, lookup_subtable_count);
        }
    }

    free(buffer);

    return true;
}

UIFontPen FontStore::get_pen()
{
    using namespace assets;
    return UIFontPen(opensans_ttf, opensans_ttf_end - opensans_ttf, m_ft_library);
}

UIFontPen FontStore::get_monospace_pen()
{
    using namespace assets;
    return UIFontPen(notomono_otf, notomono_otf_end - notomono_otf, m_ft_library);
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
    uint8_t bg_r;
    uint8_t bg_g;
    uint8_t bg_b;
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

    const int16_t offset_x = state->screen_x >= 0 ? 0 : state->screen_x;

    for (int i = 0; i < count; ++i) {
        const auto &span = spans[i];
        const auto &coverage = span.coverage;

        // Blend font colour with background
        const uint8_t r = ((coverage * pen_r) + ((255 - coverage) * state->bg_r)) >> 8;
        const uint8_t g = ((coverage * pen_g) + ((255 - coverage) * state->bg_g)) >> 8;
        const uint8_t b = ((coverage * pen_b) + ((255 - coverage) * state->bg_b)) >> 8;

        int16_t x = offset_x + state->buf_x + span.x;
        const int16_t end_x = std::min(x + span.len, state->width - 1);

        uint8_t* local_buf = x < 0 ? line_buf : line_buf + (x * 3);

        while (x < end_x) {
            if (x >= 0) {
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
        const auto &coverage = span.coverage;

        // Blend font colour with background
        const uint8_t channels[3] = {
            (uint8_t) (((coverage * pen_r) + ((255 - coverage) * state->bg_r)) >> 8),
            (uint8_t) (((coverage * pen_g) + ((255 - coverage) * state->bg_g)) >> 8),
            (uint8_t) (((coverage * pen_b) + ((255 - coverage) * state->bg_b)) >> 8),
        };

        st7789_set_cursor(state->screen_x + state->buf_x + span.x, state->screen_y + canvas_y);

        for (uint32_t k = 0; k < span.len; k++) {
            st7789_write((uint8_t*) &channels, sizeof(channels));
        }
    }
}

// Data shared between UIFontPen instances
// This is done to avoid reloading the same font for multiple pen instances.
uint8_t* UIFontPen::ms_fontdata = nullptr;
FT_Face UIFontPen::ms_face = nullptr;

UIFontPen::UIFontPen(const uint8_t* fontdata, size_t length, FT_Library library)
    : m_ft_library(library),
      m_x(0),
      m_y(0),
      m_strlen(0),
      m_colour(0xFFFFFF),
      m_background(0),
      m_size_px(16),
      m_embolden(0),
      m_mode(UIFontPen::kMode_CanvasBuffer)
{
    if (fontdata != ms_fontdata) {
        UIFontPen::unload_shared();

        FT_Error err = FT_New_Memory_Face(library, fontdata, length, 0, &ms_face);
        if (err) {
            printf("Error: Embedded font load Failed: 0x%02X\n", err);
            ms_face = nullptr;
            ms_fontdata = nullptr;
        }
    }
}

void UIFontPen::unload_shared()
{
    if (ms_face != nullptr) {
        FT_Done_Face(ms_face);
        ms_face = nullptr;
        ms_fontdata = nullptr;
    }
}

void UIFontPen::set_size(uint16_t size_px)
{
    if (ms_face == nullptr) {
        printf("Unable to set_size width as the face is in an error state\n");
        return;
    }

    m_size_px = size_px;
    FT_Set_Pixel_Sizes(ms_face, 0, size_px);
}

uint16_t UIFontPen::compute_px_width(const char* str, uint16_t length_limit)
{
    if (ms_face == nullptr) {
        printf("Unable to compute width as the face is in an error state\n");
        return 0;
    }

    uint16_t px_width = 0;

    {
        uint16_t index = 0;
        while (str[index] != '\0') {
            FT_Load_Char(ms_face, str[index], FT_LOAD_DEFAULT | FT_LOAD_BITMAP_METRICS_ONLY);
            px_width += ms_face->glyph->advance.x / 64;
            index++;

            if (length_limit != 0 && index >= length_limit) {
                // Length limit hit
                break;
            }
        }
    }

    if (px_width != 0) {
        px_width += 1;
    }

    return px_width;
}

UIRect UIFontPen::draw_length(const char* str, uint16_t length)
{
    m_strlen = length;
    return draw(str, compute_px_width(str, length));
}

UIRect UIFontPen::draw(const char* str)
{
    // Measure how width our buffer needs to be, since it's not pre-calculated
    return draw(str, compute_px_width(str));
}

UIRect UIFontPen::draw(const char* str, const uint16_t canvas_width_px)
{
    if (ms_face == nullptr) {
        printf("Unable to draw as the face is in an error state\n");
        return UIRect();
    }

    if (canvas_width_px == 0 || str == NULL || *str == '\0') {
        // Nothing to draw
        return UIRect();
    }

    // Constrain canvas to available dimensions at pen position
    const int16_t px_width = m_x >= 0
        ? std::min(DISPLAY_WIDTH -  m_x, static_cast<int>(canvas_width_px))
        : std::min(canvas_width_px + m_x, DISPLAY_WIDTH);

    const int16_t max_height = m_size_px + (m_embolden/64) - (ms_face->descender/64);
    const int16_t px_height = m_y + max_height > DISPLAY_HEIGHT
        ? DISPLAY_HEIGHT - m_y
        : max_height;

    const int baseline_correction = (px_height - max_height);

    const uint16_t canvas_bytes = px_width * px_height * 3;


    PenRasterState state;
    state.buf_x = 0;
    state.baseline = (ms_face->descender/64) - baseline_correction;
    state.screen_x = m_x;
    state.screen_y = m_y;
    state.colour = m_colour;
    state.width = px_width;
    state.height = px_height;
    state.bg_r = m_background >> 16;
    state.bg_g = m_background >> 8;
    state.bg_b = m_background;


    FT_Raster_Params params;
    memset(&params, 0, sizeof(params));
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    params.user = &state;

    if (m_mode == UIFontPen::kMode_CanvasBuffer) {
        state.buffer = (uint8_t*) malloc(canvas_bytes);
        if (state.buffer == NULL) {
            printf("Failed to allocate render buffer of %d x %d\n", px_width, px_height);
            return UIRect();
        }

        // Cheat setting background colour, as we only use greyscale backgrounds
        memset(state.buffer, state.bg_r, canvas_bytes);

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

    const int16_t offset_x = m_x >= 0 ? 0 : m_x;

    uint16_t index = 0;
    while (str[index] != '\0') {
        if (offset_x + state.buf_x >= (state.width - 1)) {
            break;
        }

        FT_Load_Char(ms_face, str[index], FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);

        const auto &slot = ms_face->glyph;

        if (slot->format == FT_GLYPH_FORMAT_OUTLINE) {
            if (m_embolden != 0) {
                FT_Outline_Embolden(&slot->outline, m_embolden);
            }

            if (m_x + state.buf_x + slot->advance.x >= 0) {
                FT_Outline_Render(m_ft_library, &slot->outline, &params);
            }
        }

        state.buf_x += slot->advance.x / 64;
        index++;

        if (m_strlen != 0 && index >= m_strlen) {
            // String length limit was provided for this draw
            m_strlen = 0;
            break;
        }
    }

    // Send rendered line to screen if needed
    if (m_mode == UIFontPen::kMode_CanvasBuffer) {
        const uint32_t render_x = m_x >= 0 ? m_x : 0;

        st7789_set_window(render_x, m_y, render_x + px_width, m_y + px_height);
        st7789_write_dma(state.buffer, px_width * px_height * 3, true);

        // Ensure writing completes before we deallocate the buffer
        st7789_deselect();

        free(state.buffer);
    }

    // Absorb the total pen movement into our state
    m_x += state.buf_x;

    return UIRect(state.screen_x, state.screen_y, canvas_width_px, state.height + 1);
}
