#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
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

    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        printf("Failed to open PTY master: %s\n", strerror(errno));
        free(font_data);
        free((void*)fb.address);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        printf("Failed to setup PTY: %s\n", strerror(errno));
        close(master_fd);
        free(font_data);
        free((void*)fb.address);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    char* slave_name = ptsname(master_fd);
    if (slave_name == NULL) {
        printf("Failed to get PTY slave name: %s\n", strerror(errno));
        close(master_fd);
        free(font_data);
        free((void*)fb.address);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    pid_t child = fork();
    if (child == 0) {
        setsid();
        int slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            _exit(1);
        }
        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            _exit(1);
        }
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO) {
            close(slave_fd);
        }
        execl("/bin/sh", "sh", NULL);
        _exit(1);
    }

    int flags = fcntl(master_fd, F_GETFL, 0);
    fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

    struct winsize ws = {
        .ws_row = (unsigned short)term.rows,
        .ws_col = (unsigned short)term.cols,
        .ws_xpixel = 0,
        .ws_ypixel = 0,
    };
    ioctl(master_fd, TIOCSWINSZ, &ws);

    SDL_StartTextInput();

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = true;
                break;
            }
            if (event.type == SDL_TEXTINPUT) {
                const char* text = event.text.text;
                write(master_fd, text, strlen(text));
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE) {
                    const char backspace = 0x7f;
                    write(master_fd, &backspace, 1);
                } else if (event.key.keysym.sym == SDLK_RETURN) {
                    const char newline = '\n';
                    write(master_fd, &newline, 1);
                }
            }
        }

        uint8_t read_buffer[256];
        ssize_t read_count = read(master_fd, read_buffer, sizeof(read_buffer));
        while (read_count > 0) {
            bt_vt_feed(&vt, read_buffer, (size_t)read_count);
            read_count = read(master_fd, read_buffer, sizeof(read_buffer));
        }

        if (child > 0) {
            int status = 0;
            pid_t result = waitpid(child, &status, WNOHANG);
            if (result == child) {
                done = true;
            }
        }

        SDL_UpdateTexture(texture, NULL, (void*)fb.address, fb.pitch);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    free(font_data);
    free((void*)fb.address);
    SDL_StopTextInput();
    if (master_fd >= 0) {
        close(master_fd);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
