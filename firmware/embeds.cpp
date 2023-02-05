#include "embeds.hh"

#define weak __attribute__((weak))

// Symbols defined by linking binary format object files
// Note these aren't pointers: they're fixed size arrays with an address defined by the linker
// These are defined as weak so pointers will initially be null in an Emscripten build.
extern "C" {
    
    weak extern const uint8_t _binary_assets_OpenSans_Regular_Stripped_ttf_start[];
    weak extern const uint8_t _binary_assets_OpenSans_Regular_Stripped_ttf_end[];

    weak extern const uint8_t _binary_assets_NotoSansMono_Regular_Stripped_otf_start[];
    weak extern const uint8_t _binary_assets_NotoSansMono_Regular_Stripped_otf_end[];

    weak extern const uint8_t _binary_assets_unicode_logo_png_start[];
    weak extern const uint8_t _binary_assets_unicode_logo_png_end[];
}

// Pointers that to the memory above, whereever it ends up at link time
namespace assets {

const uint8_t* opensans_ttf = _binary_assets_OpenSans_Regular_Stripped_ttf_start;
const uint8_t* opensans_ttf_end = _binary_assets_OpenSans_Regular_Stripped_ttf_end;

const uint8_t* notomono_otf = _binary_assets_NotoSansMono_Regular_Stripped_otf_start;
const uint8_t* notomono_otf_end = _binary_assets_NotoSansMono_Regular_Stripped_otf_end;

const uint8_t* unicode_logo_png = _binary_assets_unicode_logo_png_start;
const uint8_t* unicode_logo_png_end = _binary_assets_unicode_logo_png_end;

} // namespace assets