#ifndef __BT_H__
#define __BT_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dazzle.h>
#include <dt_glyphs.h>

#ifdef __BT_IMPL__
#define __UTF8_IMPL__
#endif
#include <utf8.h>
#ifdef __BT_IMPL__
#undef __UTF8_IMPL__
#endif

typedef struct {
    dazzle_context_t* ctx;
    font_t font;
    uint32_t width;
    uint32_t height;
    uint32_t cols;
    uint32_t rows;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t fg_color;
    uint32_t bg_color;
} bt_terminal_t;

void bt_terminal_init(bt_terminal_t* term, dazzle_context_t* ctx, font_t font, uint32_t width, uint32_t height);
void bt_terminal_set_colors(bt_terminal_t* term, uint32_t fg_color, uint32_t bg_color);
void bt_terminal_set_cursor(bt_terminal_t* term, uint32_t col, uint32_t row);
void bt_terminal_set_font(bt_terminal_t* term, font_t font);
void bt_terminal_clear(bt_terminal_t* term);
bool bt_terminal_put_index(bt_terminal_t* term, uint32_t index);
bool bt_terminal_putc(bt_terminal_t* term, char c);
bool bt_terminal_write(bt_terminal_t* term, const char* text);

#ifdef __BT_IMPL__

static void bt_terminal_recalc(bt_terminal_t* term) {
    if (term->font.suggested_width == 0 || term->font.suggested_height == 0) {
        term->cols = 0;
        term->rows = 0;
        return;
    }
    term->cols = term->width / term->font.suggested_width;
    term->rows = term->height / term->font.suggested_height;
}

void bt_terminal_init(bt_terminal_t* term, dazzle_context_t* ctx, font_t font, uint32_t width, uint32_t height) {
    term->ctx = ctx;
    term->font = font;
    term->width = width;
    term->height = height;
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->fg_color = 0xFFFFFFFF;
    term->bg_color = 0x00000000;
    bt_terminal_recalc(term);
}

void bt_terminal_set_colors(bt_terminal_t* term, uint32_t fg_color, uint32_t bg_color) {
    term->fg_color = fg_color;
    term->bg_color = bg_color;
}

void bt_terminal_set_cursor(bt_terminal_t* term, uint32_t col, uint32_t row) {
    term->cursor_x = col;
    term->cursor_y = row;
}

void bt_terminal_set_font(bt_terminal_t* term, font_t font) {
    term->font = font;
    bt_terminal_recalc(term);
}

void bt_terminal_clear(bt_terminal_t* term) {
    term->cursor_x = 0;
    term->cursor_y = 0;
    dazzle_clear(term->ctx, term->bg_color);
}

static void bt_terminal_newline(bt_terminal_t* term) {
    term->cursor_x = 0;
    term->cursor_y++;
    if (term->rows != 0 && term->cursor_y >= term->rows) {
        term->cursor_y = 0;
        dazzle_clear(term->ctx, term->bg_color);
    }
}

bool bt_terminal_put_index(bt_terminal_t* term, uint32_t index) {
    if (term->cols == 0 || term->rows == 0) {
        return false;
    }
    if (index >= term->font.glyph_count) {
        index = 0;
    }

    uint32_t cell_width = term->font.suggested_width;
    uint32_t cell_height = term->font.suggested_height;
    uint32_t px = term->cursor_x * cell_width;
    uint32_t py = term->cursor_y * cell_height;

    dazzle_retained_element_t* bg = dazzle_create_rectangle(term->ctx, px, py, cell_width, cell_height, true, term->bg_color);
    dazzle_draw(term->ctx, bg);

    glyph_t glyph = render_glyph(term->font, index, 0, term->fg_color);
    dazzle_retained_element_t* blit = dazzle_create_blitable(term->ctx, px, py, glyph.width, glyph.height, glyph.buffer);
    dazzle_draw(term->ctx, blit);
    term->font.alloc.free(glyph.buffer);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        bt_terminal_newline(term);
    }
    return true;
}

bool bt_terminal_putc(bt_terminal_t* term, char c) {
    if (c == '\n') {
        bt_terminal_newline(term);
        return true;
    }
    return bt_terminal_put_index(term, (uint8_t)c);
}

bool bt_terminal_write(bt_terminal_t* term, const char* text) {
    if (text == NULL) {
        return false;
    }
    utf8_dec_state_t state = {0};
    for (const uint8_t* cursor = (const uint8_t*)text; *cursor != '\0'; cursor++) {
        uint32_t codepoint = 0;
        uint8_t result = utf8_decode(&state, *cursor, &codepoint);
        if (result == UTF8_MORE_BYTES_REQUIRED) {
            continue;
        }
        if (result == UTF8_INVALID_INPUT) {
            state.bytes_remaining = 0;
            codepoint = '?';
        }
        if (codepoint == '\n') {
            if (!bt_terminal_putc(term, '\n')) {
                return false;
            }
            continue;
        }
        if (!bt_terminal_put_index(term, codepoint)) {
            return false;
        }
    }
    return true;
}

#endif // __BT_IMPL__

#endif // __BT_H__
