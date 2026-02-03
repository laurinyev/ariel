#ifndef __BT_H__
#define __BT_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

typedef enum {
    BT_VT_STATE_TEXT,
    BT_VT_STATE_ESC,
    BT_VT_STATE_CSI
} bt_vt_state_t;

typedef struct {
    bt_terminal_t* term;
    bt_vt_state_t state;
    utf8_dec_state_t utf8;
    uint8_t csi_count;
    uint32_t csi_params[8];
    uint32_t csi_current;
    bool csi_has_current;
} bt_vt_t;

void bt_vt_init(bt_vt_t* vt, bt_terminal_t* term);
bool bt_vt_putc(bt_vt_t* vt, uint8_t byte);
bool bt_vt_feed(bt_vt_t* vt, const uint8_t* data, size_t length);
bool bt_vt_write(bt_vt_t* vt, const char* text);

#ifdef __BT_IMPL__

static const uint32_t bt_vt_ansi_colors[8] = {
    0x000000FF,
    0xFF0000FF,
    0x00FF00FF,
    0xFFFF00FF,
    0x0000FFFF,
    0xFF00FFFF,
    0x00FFFFFF,
    0xFFFFFFFF
};

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

static void bt_terminal_clear_line(bt_terminal_t* term, uint32_t row) {
    if (term->cols == 0 || term->rows == 0) {
        return;
    }
    if (row >= term->rows) {
        return;
    }
    uint32_t cell_width = term->font.suggested_width;
    uint32_t cell_height = term->font.suggested_height;
    uint32_t px = 0;
    uint32_t py = row * cell_height;
    dazzle_retained_element_t* bg = dazzle_create_rectangle(
        term->ctx,
        px,
        py,
        cell_width * term->cols,
        cell_height,
        true,
        term->bg_color
    );
    dazzle_draw(term->ctx, bg);
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

static void bt_vt_reset_csi(bt_vt_t* vt) {
    vt->csi_count = 0;
    vt->csi_current = 0;
    vt->csi_has_current = false;
}

static uint32_t bt_vt_get_param(bt_vt_t* vt, uint8_t index, uint32_t fallback) {
    if (index >= vt->csi_count) {
        return fallback;
    }
    return vt->csi_params[index] == 0 ? fallback : vt->csi_params[index];
}

static void bt_vt_store_param(bt_vt_t* vt) {
    if (vt->csi_count >= 8) {
        vt->csi_has_current = false;
        vt->csi_current = 0;
        return;
    }
    vt->csi_params[vt->csi_count++] = vt->csi_has_current ? vt->csi_current : 0;
    vt->csi_current = 0;
    vt->csi_has_current = false;
}

static bool bt_vt_handle_csi(bt_vt_t* vt, uint8_t final) {
    bt_terminal_t* term = vt->term;
    if (vt->csi_has_current || vt->csi_count > 0) {
        bt_vt_store_param(vt);
    }
    switch (final) {
        case 'A': {
            uint32_t n = bt_vt_get_param(vt, 0, 1);
            if (term->cursor_y < n) {
                term->cursor_y = 0;
            } else {
                term->cursor_y -= n;
            }
            break;
        }
        case 'B': {
            uint32_t n = bt_vt_get_param(vt, 0, 1);
            term->cursor_y += n;
            if (term->rows > 0 && term->cursor_y >= term->rows) {
                term->cursor_y = term->rows - 1;
            }
            break;
        }
        case 'C': {
            uint32_t n = bt_vt_get_param(vt, 0, 1);
            term->cursor_x += n;
            if (term->cols > 0 && term->cursor_x >= term->cols) {
                term->cursor_x = term->cols - 1;
            }
            break;
        }
        case 'D': {
            uint32_t n = bt_vt_get_param(vt, 0, 1);
            if (term->cursor_x < n) {
                term->cursor_x = 0;
            } else {
                term->cursor_x -= n;
            }
            break;
        }
        case 'H':
        case 'f': {
            uint32_t row = bt_vt_get_param(vt, 0, 1);
            uint32_t col = bt_vt_get_param(vt, 1, 1);
            if (row > 0) {
                row -= 1;
            }
            if (col > 0) {
                col -= 1;
            }
            bt_terminal_set_cursor(term, col, row);
            break;
        }
        case 'J': {
            bt_terminal_clear(term);
            break;
        }
        case 'K': {
            bt_terminal_clear_line(term, term->cursor_y);
            break;
        }
        case 'm': {
            if (vt->csi_count == 0) {
                term->fg_color = 0xFFFFFFFF;
                term->bg_color = 0x00000000;
                break;
            }
            for (uint8_t i = 0; i < vt->csi_count; i++) {
                uint32_t param = vt->csi_params[i];
                if (param == 0) {
                    term->fg_color = 0xFFFFFFFF;
                    term->bg_color = 0x00000000;
                } else if (param == 39) {
                    term->fg_color = 0xFFFFFFFF;
                } else if (param == 49) {
                    term->bg_color = 0x00000000;
                } else if (param >= 30 && param <= 37) {
                    term->fg_color = bt_vt_ansi_colors[param - 30];
                } else if (param >= 40 && param <= 47) {
                    term->bg_color = bt_vt_ansi_colors[param - 40];
                }
            }
            break;
        }
        default:
            break;
    }
    bt_vt_reset_csi(vt);
    return true;
}

void bt_vt_init(bt_vt_t* vt, bt_terminal_t* term) {
    vt->term = term;
    vt->state = BT_VT_STATE_TEXT;
    vt->utf8 = (utf8_dec_state_t){0};
    bt_vt_reset_csi(vt);
}

bool bt_vt_putc(bt_vt_t* vt, uint8_t byte) {
    bt_terminal_t* term = vt->term;
    if (vt->state == BT_VT_STATE_TEXT) {
        if (byte == 0x1B) {
            vt->utf8.bytes_remaining = 0;
            vt->state = BT_VT_STATE_ESC;
            return true;
        }
        if (byte == '\r') {
            vt->utf8.bytes_remaining = 0;
            term->cursor_x = 0;
            return true;
        }
        if (byte == '\n') {
            vt->utf8.bytes_remaining = 0;
            return bt_terminal_putc(term, '\n');
        }
        if (byte == '\b') {
            vt->utf8.bytes_remaining = 0;
            if (term->cursor_x > 0) {
                term->cursor_x--;
            }
            return true;
        }
        uint32_t codepoint = 0;
        uint8_t result = utf8_decode(&vt->utf8, byte, &codepoint);
        if (result == UTF8_MORE_BYTES_REQUIRED) {
            return true;
        }
        if (result == UTF8_INVALID_INPUT) {
            vt->utf8.bytes_remaining = 0;
            codepoint = '?';
        }
        return bt_terminal_put_index(term, codepoint);
    }
    if (vt->state == BT_VT_STATE_ESC) {
        if (byte == '[') {
            vt->state = BT_VT_STATE_CSI;
            bt_vt_reset_csi(vt);
            return true;
        }
        if (byte == 'c') {
            bt_terminal_clear(term);
        }
        vt->state = BT_VT_STATE_TEXT;
        return true;
    }
    if (vt->state == BT_VT_STATE_CSI) {
        if (byte >= '0' && byte <= '9') {
            vt->csi_current = vt->csi_current * 10 + (byte - '0');
            vt->csi_has_current = true;
            return true;
        }
        if (byte == ';') {
            bt_vt_store_param(vt);
            return true;
        }
        bt_vt_handle_csi(vt, byte);
        vt->state = BT_VT_STATE_TEXT;
        return true;
    }
    return false;
}

bool bt_vt_feed(bt_vt_t* vt, const uint8_t* data, size_t length) {
    if (data == NULL) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (!bt_vt_putc(vt, data[i])) {
            return false;
        }
    }
    return true;
}

bool bt_vt_write(bt_vt_t* vt, const char* text) {
    if (text == NULL) {
        return false;
    }
    return bt_vt_feed(vt, (const uint8_t*)text, strlen(text));
}

#endif // __BT_IMPL__

#endif // __BT_H__
