#include "main_ui.hh"
#include "st7789.h"

#include "SDL.h"

#include <thread>
#include <atomic>

static std::atomic_bool app_terminated = false;

static SDL_Window* screen = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screen_texture = NULL;
static uint32_t* px_buffer = NULL;

static MainUI* app_ptr;

void application()
{
    printf("Starting MainUI...\n");
    MainUI app;
    app_ptr = &app;

    app.run_demo();

    printf("MainUI finished\n");
    app_terminated = true;
    app_ptr = nullptr;
}

void handle_keypress(MainUI &app, const SDL_KeyboardEvent &key)
{
    switch (key.keysym.scancode) {

        // Toggle data input switches using the bit number
        case SDL_SCANCODE_0:
        case SDL_SCANCODE_KP_0:
            break;

        case SDL_SCANCODE_1:
        case SDL_SCANCODE_KP_1:
            break;

        case SDL_SCANCODE_2:
        case SDL_SCANCODE_KP_2:
            break;

        case SDL_SCANCODE_3:
        case SDL_SCANCODE_KP_3:
            break;

        case SDL_SCANCODE_4:
        case SDL_SCANCODE_KP_4:
            break;

        case SDL_SCANCODE_5:
        case SDL_SCANCODE_KP_5:
            break;

        case SDL_SCANCODE_6:
        case SDL_SCANCODE_KP_6:
            break;

        case SDL_SCANCODE_7:
        case SDL_SCANCODE_KP_7:
            break;

        // Clear/Mode switch
        case SDL_SCANCODE_DELETE:
            break;

        // Shift switch
        case SDL_SCANCODE_KP_PLUS:
        case SDL_SCANCODE_PAGEDOWN:
            break;

        // Send switch
        case SDL_SCANCODE_KP_ENTER:
        case SDL_SCANCODE_RETURN:
            break;
    }
}

int main()
{
    const uint kDisplayScaling = 1;

    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Could not init SDL: %s\n", SDL_GetError());
        return 1;
    }

    screen = SDL_CreateWindow("Screen",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        DISPLAY_WIDTH * kDisplayScaling, DISPLAY_HEIGHT * kDisplayScaling, 0
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
    std::thread app_thread(application);

    bool needs_render = true;
    SDL_AddTimer(16 /* milliseconds */, [](Uint32 interval, void *param) -> Uint32 {
        bool* needs_render = (bool*) param;
        *needs_render = true;
        return interval;
    }, &needs_render);

    while (!app_terminated) {

        // Update display if needed
        if (needs_render) {
            needs_render = false;
            SDL_UpdateTexture(screen_texture, NULL, px_buffer, DISPLAY_WIDTH * 4);
            SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // Poll for events (loops until there are no more events to process)
        SDL_Event event;
        while( SDL_PollEvent( &event ) ){
            if (event.type == SDL_KEYDOWN) {
                // Ignore repeats
                if (event.key.repeat) {
                    continue;
                }

                handle_keypress(*app_ptr, event.key);
            }
        }
    }

    app_thread.join();

    free(px_buffer);

    SDL_DestroyWindow(screen);
    SDL_Quit();

    return 0;
}
