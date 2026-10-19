#include "terminal_input.h"
#include <stdio.h>
#include <string.h>
size_t terminal_encode_key(TerminalKey key, unsigned modifiers,
                           unsigned char *output, size_t capacity)
{
    static const unsigned tilde[] = {15, 17, 18, 19, 20, 21, 23, 24};
    char bytes[16], final = 0;
    unsigned number = 0, mod = 1 + (modifiers & 7);
    size_t n = 0;
    switch (key) {
    case TERMINAL_KEY_ESCAPE: bytes[n++] = '\033'; break;
    case TERMINAL_KEY_ENTER: bytes[n++] = '\r'; break;
    case TERMINAL_KEY_BACKSPACE: bytes[n++] = '\177'; break;
    case TERMINAL_KEY_TAB:
        if (modifiers & TERMINAL_SHIFT) {
            if (mod == 2) { memcpy(bytes, "\033[Z", 3); n = 3; }
            else { final = 'Z'; }
        } else bytes[n++] = '\t';
        break;
    case TERMINAL_KEY_UP: final = 'A'; break;
    case TERMINAL_KEY_DOWN: final = 'B'; break;
    case TERMINAL_KEY_RIGHT: final = 'C'; break;
    case TERMINAL_KEY_LEFT: final = 'D'; break;
    case TERMINAL_KEY_HOME: final = 'H'; break;
    case TERMINAL_KEY_END: final = 'F'; break;
    case TERMINAL_KEY_INSERT: number = 2; break;
    case TERMINAL_KEY_DELETE: number = 3; break;
    case TERMINAL_KEY_PAGEUP: number = 5; break;
    case TERMINAL_KEY_PAGEDOWN: number = 6; break;
    default:
        if (key >= TERMINAL_KEY_F1 && key <= TERMINAL_KEY_F4)
            final = (char)('P' + key - TERMINAL_KEY_F1);
        else if (key >= TERMINAL_KEY_F5 && key <= TERMINAL_KEY_F12)
            number = tilde[key - TERMINAL_KEY_F5];
        else return 0;
    }
    if (final) {
        if (mod > 1) n = (size_t)snprintf(bytes, sizeof bytes, "\033[1;%u%c", mod, final);
        else n = (size_t)snprintf(bytes, sizeof bytes, "\033%c%c",
                                  key >= TERMINAL_KEY_F1 ? 'O' : '[', final);
    } else if (number) {
        if (mod > 1) n = (size_t)snprintf(bytes, sizeof bytes, "\033[%u;%u~", number, mod);
        else n = (size_t)snprintf(bytes, sizeof bytes, "\033[%u~", number);
    } else if (n == 1 && (modifiers & TERMINAL_ALT)) {
        bytes[1] = bytes[0]; bytes[0] = '\033'; n = 2;
    }
    if (!output || n > capacity) return 0;
    memcpy(output, bytes, n);
    return n;
}

size_t terminal_encode_modified_enter(unsigned modifiers, unsigned char *output, size_t capacity)
{
    static const char seq[] = "\033[27;2;13~";
    if (modifiers != TERMINAL_SHIFT || !output || capacity < sizeof seq - 1) return 0;
    memcpy(output, seq, sizeof seq - 1);
    return sizeof seq - 1;
}

static unsigned char control(unsigned char c, unsigned modifiers)
{
    if (!(modifiers & TERMINAL_CTRL)) return c;
    if ((c >= '@' && c <= '_') || (c >= 'a' && c <= 'z')) return c & 31;
    if (c == ' ' || c == '2') return 0;
    if (c == '?') return 127;
    return c;
}
static unsigned code_point(unsigned char c, const unsigned short *high)
{
    if (c < 128 || !high || !high[c - 128]) return c;
    return high[c - 128];
}

static size_t utf8_width(unsigned u)
{
    return u < 0x80 ? 1 : u < 0x800 ? 2 : 3;
}

size_t terminal_encode_charset(const unsigned char *bytes, size_t length,
                               const unsigned short *high, unsigned modifiers,
                               unsigned char *output, size_t capacity)
{
    size_t i, n = (modifiers & TERMINAL_ALT) ? 1 : 0, used = 0;
    if (!bytes || !length || !output || n > capacity) return 0;
    for (i = 0; i < length; ++i) {
        size_t width = utf8_width(code_point(control(bytes[i], modifiers), high));
        if (width > capacity - n) return 0;
        n += width;
    }
    if (modifiers & TERMINAL_ALT) output[used++] = '\033';
    for (i = 0; i < length; ++i) {
        unsigned u = code_point(control(bytes[i], modifiers), high);
        if (u < 0x80) output[used++] = (unsigned char)u;
        else if (u < 0x800) {
            output[used++] = (unsigned char)(0xc0 | (u >> 6));
            output[used++] = (unsigned char)(0x80 | (u & 63));
        } else {
            output[used++] = (unsigned char)(0xe0 | (u >> 12));
            output[used++] = (unsigned char)(0x80 | ((u >> 6) & 63));
            output[used++] = (unsigned char)(0x80 | (u & 63));
        }
    }
    return used;
}

size_t terminal_encode_latin1(const unsigned char *bytes, size_t length,
                              unsigned modifiers, unsigned char *output,
                              size_t capacity)
{
    return terminal_encode_charset(bytes, length, NULL, modifiers, output, capacity);
}
