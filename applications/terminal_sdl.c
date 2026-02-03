#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#define __DAZZLE_IMPL__
#define __BT_IMPL__

#include <bt.h>

static uint8_t* read_file(const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return NULL;
    }
    fseek(file, 0, SEEK_SET);
    uint8_t* buffer = malloc((size_t)size);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(buffer, (size_t)size, 1, file) != 1) {
        fclose(file);
        free(buffer);
        return NULL;
    }
    fclose(file);
    *out_size = (size_t)size;
    return buffer;
}

int main(int argc, char** argv) {
    const char* font_path = "../tests/test.psf";
    if (argc > 1) {
        font_path = argv[1];
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "BetterM VT Demo",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        600,
        0
    );
    if (window == NULL) {
        printf("Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 800, 600);
    if (texture == NULL) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    dazzle_allocator_t alloc = {
        .malloc = malloc,
        .free = free,
    };

    dazzle_framebuffer_t fb = {
        .address = (uintptr_t)malloc(800 * 600 * 4),
        .width = 800,
        .height = 600,
        .pitch = 4 * 800,
        .bpp = 32,
        .red_mask = 0xFF,
        .green_mask = 0xFF,
        .blue_mask = 0xFF,
        .alpha_mask = 0xFF,
        .red_shift = 24,
        .green_shift = 16,
        .blue_shift = 8,
        .alpha_shift = 0,
    };

    dazzle_context_t* ctx = dazzle_init_fb(alloc, &fb);
    if (ctx == NULL) {
        printf("Failed to initialize dazzle context\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    size_t font_size = 0;
    uint8_t* font_data = read_file(font_path, &font_size);
    if (font_data == NULL) {
        printf("Failed to load font: %s\n", font_path);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    font_t font = load_font(alloc, (char*)font_data, (uint32_t)font_size);

    bt_terminal_t term;
    bt_terminal_init(&term, ctx, font, fb.width, fb.height);
    bt_terminal_set_colors(&term, 0xFFFFFFFF, 0x00000000);
    bt_terminal_clear(&term);

    bt_vt_t vt;
    bt_vt_init(&vt, &term);

    const char* banner =
        "BetterM VT demo\n"
        "\x1b[32mGreen\x1b[0m \x1b[31mRed\x1b[0m \x1b[34mBlue\x1b[0m\n"
        "Cursor test: \x1b[5;10HHere!\n"
        "\x1b[3;1HUTF-8: snowman \xE2\x98\x83 and coffee \xE2\x98\x95\n";

    bt_vt_write(&vt, banner);

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = true;
                break;
            }
        }

        SDL_UpdateTexture(texture, NULL, (void*)fb.address, fb.pitch);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    free(font_data);
    free((void*)fb.address);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
