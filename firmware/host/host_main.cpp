#include "st7789.h"
#include "ui/main_ui.hh"
#include "util.hh"

#include "SDL.h"

#include <atomic>
#include <thread>

static std::atomic_bool app_terminated = false;

static SDL_Window* screen = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screen_texture = NULL;
static uint32_t* px_buffer = NULL;

static uint8_t binary_input = 0;
static SDL_TimerID modeclear_pending = 0;
static SDL_TimerID shift_pending = 0;

static MainUI* app = nullptr;
static bool is_app_valid = false;
static bool needs_render = true;
static SDL_Rect vscreen_dest;

const char* s_font_path = nullptr;

#ifdef EMSCRIPTEN
// Emscripten doesn't support embedding assets directly in the binary
// We emulate this by loading from the virtual filesystem during start instead
#include <emscripten.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "embeds.hh"

std::vector<std::string> _loaded_files;

void load_embed(const char* path, const uint8_t** start, const uint8_t** end)
{
    std::ifstream t(path, std::ios::binary);
    std::stringstream buffer;
    buffer << t.rdbuf();

    auto &contents = _loaded_files.emplace_back(buffer.str());

    *start = (const uint8_t*) contents.c_str();
    *end = (*start) + contents.size();
}

void load_binary_embeds(void)
{
    using namespace assets;
    load_embed("assets/OpenSans-Regular-Stripped.ttf", &opensans_ttf, &opensans_ttf_end);
    load_embed("assets/NotoSansMono-Regular-Stripped.otf", &notomono_otf, &notomono_otf_end);
    load_embed("assets/unicode-logo.png", &unicode_logo_png, &unicode_logo_png_end);
}
#endif

void app_load()
{
    printf("Starting MainUI...\n");
    is_app_valid = app->load(s_font_path);
}

void handle_keydown(const SDL_KeyboardEvent &key)
{
    switch (key.keysym.scancode) {

        // Toggle data input switches using the bit number
        case SDL_SCANCODE_0:
        case SDL_SCANCODE_KP_0:
        case SDL_SCANCODE_F12:
            binary_input ^= (1<<0);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_1:
        case SDL_SCANCODE_KP_1:
        case SDL_SCANCODE_F11:
            binary_input ^= (1<<1);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_2:
        case SDL_SCANCODE_KP_2:
        case SDL_SCANCODE_F10:
            binary_input ^= (1<<2);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_3:
        case SDL_SCANCODE_KP_3:
        case SDL_SCANCODE_F9:
            binary_input ^= (1<<3);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_4:
        case SDL_SCANCODE_KP_4:
        case SDL_SCANCODE_F8:
            binary_input ^= (1<<4);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_5:
        case SDL_SCANCODE_KP_5:
        case SDL_SCANCODE_F7:
            binary_input ^= (1<<5);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_6:
        case SDL_SCANCODE_KP_6:
        case SDL_SCANCODE_F6:
            binary_input ^= (1<<6);
            app->set_low_byte(binary_input);
            break;

        case SDL_SCANCODE_7:
        case SDL_SCANCODE_KP_7:
        case SDL_SCANCODE_F5:
            binary_input ^= (1<<7);
            app->set_low_byte(binary_input);
            break;

        // Helper key to zero all switches
        case SDL_SCANCODE_F4:
            binary_input = 0;
            app->set_low_byte(binary_input);
            break;

        // Clear/Mode switch
        case SDL_SCANCODE_DELETE:
            // Clear when held
            modeclear_pending = SDL_AddTimer(500 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
                modeclear_pending = 0;
                app->reset();
                return 0;
            }, nullptr);

            break;

        // Shift switch
        case SDL_SCANCODE_INSERT:
        case SDL_SCANCODE_KP_PLUS:
            // Shift-lock when held
            shift_pending = SDL_AddTimer(500 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
                shift_pending = 0;
                app->toggle_shift_lock();
                return 0;
            }, nullptr);

            break;

        // Send switch
        case SDL_SCANCODE_KP_ENTER:
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_END:
            std::string output;
            for (uint32_t codepoint : app->get_codepoints()) {
                const char* encoding = codepoint_to_utf8(codepoint);
                if (encoding == nullptr) {
                    printf("Could not encode codepoint %u as UTF-8\n", codepoint);
                    continue;
                }

                output += encoding;
            }

            printf("Sent: %s\n", output.c_str());
            app->flush_buffer();
            break;
    }
}

void handle_keyup(const SDL_KeyboardEvent &key)
{
    switch (key.keysym.scancode) {

        // Clear/Mode switch
        case SDL_SCANCODE_DELETE:
            if (modeclear_pending) {
                SDL_RemoveTimer(modeclear_pending);
                modeclear_pending = 0;
                app->goto_next_mode(binary_input);
            }
            break;

        // Shift switch
        case SDL_SCANCODE_INSERT:
        case SDL_SCANCODE_KP_PLUS:
            if (shift_pending) {
                SDL_RemoveTimer(shift_pending);
                shift_pending = 0;
                app->shift();
            }
            break;
    }
}

void handle_event(SDL_Event& event)
{
    switch (event.type) {
        case SDL_KEYDOWN:
            // Ignore repeats
            if (event.key.repeat) {
                return;
            }

            handle_keydown(event.key);
            break;

        case SDL_KEYUP:
            // Ignore repeats
            if (event.key.repeat) {
                return;
            }

            handle_keyup(event.key);
            break;

        case SDL_QUIT:
            printf("Caught quit signal...\n");
            app_terminated = true;
            break;
    }
}

void main_loop()
{
    // Update display if needed
    if (needs_render) {
        needs_render = false;

        // Set border colour
        SDL_SetRenderDrawColor(renderer, 205, 205, 205, 255);
        SDL_RenderClear(renderer);

        // Update switch indicators
        {
            int total_width = 0;
            int total_height = 0;
            SDL_GetWindowSize(screen, &total_width, &total_height);

            SDL_Rect indicator;
            indicator.w = 15;
            indicator.h = 15;
            indicator.x = 0;
            indicator.y = total_height - 20;

            const int indicator_spacing = (total_width - indicator.w)/8;

            for (uint32_t i = 0; i < 8; i++) {
                indicator.x = (indicator_spacing * (7 - i)) + (indicator_spacing/2);

                if ((binary_input >> i) & 1) {
                    SDL_SetRenderDrawColor(renderer, 200, 10, 0, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 110, 100, 100, 255);
                }

                SDL_RenderFillRect(renderer, &indicator);
            }
        }

        // Update virtual display
        SDL_UpdateTexture(screen_texture, NULL, px_buffer, DISPLAY_WIDTH * 4);
        SDL_RenderCopy(renderer, screen_texture, NULL, &vscreen_dest);
        SDL_RenderPresent(renderer);
    }

    // Poll for events (loops until there are no more events to process)
    SDL_Event event;
    while( SDL_WaitEventTimeout( &event, 1) ){
        if (is_app_valid) {
            handle_event(event);
        }
    }
}

int main(int argc, const char* argv[])
{
#ifdef EMSCRIPTEN
    s_font_path = "assets/fonts";
    load_binary_embeds();
    emscripten_set_main_loop(main_loop, 0, 0);
#else
    if (argc < 2) {
        printf("Usage:\n  %s <fonts-dir>\n", argv[0]);
        return 1;
    }

    s_font_path = argv[1];
#endif

    const uint32_t kDisplayScaling = 2;
    const uint32_t kDisplayPadding = 50;

    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Could not init SDL: %s\n", SDL_GetError());
        return 1;
    }

    screen = SDL_CreateWindow("Screen",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        DISPLAY_WIDTH * kDisplayScaling + kDisplayPadding * 2, DISPLAY_HEIGHT * kDisplayScaling + kDisplayPadding * 2, 0
    );

    if(!screen) {
        fprintf(stderr, "Could not create window\n");
        return 1;
    }

    // Create a renderer with V-Sync enabled.
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) {
        fprintf(stderr, "Could not create renderer\n");
        return 1;
    }

    // Create a streaming texture of size 320 x 240 with nearest-neighbor scaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    screen_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_WIDTH, DISPLAY_HEIGHT
    );

    // Init virtual display implementation
    st7789_init(&px_buffer, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Update the virtual display from the pixel data at 60Hz
    // This approximates how the actual display device works
    SDL_AddTimer(1000/60 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
        bool* needs_render = (bool*) param;
        *needs_render = true;
        return interval;
    }, &needs_render);

    // Make the application do any periodic updates
    SDL_AddTimer(1000/30 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
        if (is_app_valid) app->tick();
        return interval;
    }, nullptr);

    // Location to render virtual screen
    vscreen_dest.x = kDisplayPadding;
    vscreen_dest.y = kDisplayPadding;
    vscreen_dest.w = DISPLAY_WIDTH * kDisplayScaling;
    vscreen_dest.h = DISPLAY_HEIGHT * kDisplayScaling;

    // Create the application
    app = new MainUI();

#ifdef EMSCRIPTEN
    // TODO: Write a version of load that doesn't block the UI, or is web-specific
    app_load();
#else
    // Perform the initial blocking load in a thread
    // Needed as this also updates the display, which won't happen if the main thread is occupied
    std::thread load_thread(app_load);

    while (!app_terminated) {
        main_loop();
    }

    // Wait for any background task that's still running
    load_thread.join();

    SDL_DestroyWindow(screen);
    SDL_Quit();

    free(px_buffer);
#endif

    return 0;
}
