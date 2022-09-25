#include "main_ui.hh"
#include "st7789.h"

#include "SDL.h"

#include <atomic>
#include <thread>

static std::atomic_bool app_terminated = false;

static SDL_Window* screen = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screen_texture = NULL;
static uint32_t* px_buffer = NULL;

static uint8_t binary_input = 0;
static bool modeclear_pending = false;
static bool shift_pending = false;

static MainUI* app = nullptr;

void init_app()
{
    printf("Starting MainUI...\n");
    app = new MainUI();
}

std::string codepoint_to_utf8(uint32_t codepoint)
{
    uint8_t stack[4];
    uint index = 0;

    if (codepoint <= 0x7F) {
        stack[index++] = codepoint;
    } else if (codepoint <= 0x7FF) {
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0xC0 | (codepoint & 0x1F);
    } else if (codepoint <= 0xFFFF) {
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0xE0 | (codepoint & 0x0F);
    } else if (codepoint <= 0x1FFFFF) {
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0x80 | (codepoint & 0x3F); codepoint >>= 6;
        stack[index++] = 0xF0 | (codepoint & 0x07);
    } else {
        return std::string("Failed to encode as utf-8: ") + std::to_string(codepoint);
    }

    std::string buf;
    buf.reserve(index + 1);

    const uint numbytes = index;
    for (uint i = 0; i < numbytes; i++) {
        buf[i] = stack[--index];
    }

    buf[numbytes] = '\0';

    return buf;
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

        // Clear/Mode switch
        case SDL_SCANCODE_DELETE:
            modeclear_pending = true;

            // Clear when held
            SDL_AddTimer(500 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
                if (modeclear_pending) {
                    modeclear_pending = false;
                    app->reset();
                }
                return 0;
            }, nullptr);

            break;

        // Shift switch
        case SDL_SCANCODE_INSERT:
        case SDL_SCANCODE_KP_PLUS:
            shift_pending = true;

            // Shift-lock when held
            SDL_AddTimer(500 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
                if (shift_pending) {
                    shift_pending = false;

                    if (app->get_shift_lock()) {
                        app->set_shift_lock(false);
                        printf("Cleared shift-lock\n");
                    } else {
                        app->set_shift_lock(true);
                        printf("Enabled shift-lock\n");
                    }
                }
                return 0;
            }, nullptr);

            break;

        // Send switch
        case SDL_SCANCODE_KP_ENTER:
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_END:
            printf("Sent: %s\n", codepoint_to_utf8(app->get_codepoint()).c_str());
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
                modeclear_pending = false;
                // TODO: Change display mode
                printf("Would change display mode\n");
            }
            break;

        // Shift switch
        case SDL_SCANCODE_INSERT:
        case SDL_SCANCODE_KP_PLUS:
            if (shift_pending) {
                shift_pending = false;
                app->shift();
            }
            break;
    }
}

int main()
{
    const uint kDisplayScaling = 2;
    const uint kDisplayPadding = 50;

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
    bool needs_render = true;
    SDL_AddTimer(1000/60 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
        bool* needs_render = (bool*) param;
        *needs_render = true;
        return interval;
    }, &needs_render);

    // Make the application do any periodic updates
    SDL_AddTimer(1000/30 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
        if (app != nullptr) app->tick();
        return interval;
    }, nullptr);

    // Location to render virtual screen
    SDL_Rect vscreen_dest;
    vscreen_dest.x = kDisplayPadding;
    vscreen_dest.y = kDisplayPadding;
    vscreen_dest.w = DISPLAY_WIDTH * kDisplayScaling;
    vscreen_dest.h = DISPLAY_HEIGHT * kDisplayScaling;

    // Start application on a background thread
    // Needed as initialisation is a blocking process that also updates the display
    std::thread app_thread(init_app);

    while (!app_terminated) {

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

                for (uint i = 0; i < 8; i++) {
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
        while( SDL_PollEvent( &event ) ){
            switch (event.type) {
                case SDL_KEYDOWN:
                    // Ignore repeats
                    if (event.key.repeat) {
                        continue;
                    }

                    handle_keydown(event.key);
                    break;

                case SDL_KEYUP:
                    // Ignore repeats
                    if (event.key.repeat) {
                        continue;
                    }

                    handle_keyup(event.key);
                    break;

                case SDL_QUIT:
                    printf("Caught quit signal...\n");
                    app_terminated = true;
                    break;
            }
        }
    }

    // Wait for any background task that's still running
    app_thread.join();

    SDL_DestroyWindow(screen);
    SDL_Quit();

    free(px_buffer);

    return 0;
}
