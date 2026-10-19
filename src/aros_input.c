#include "aros_input.h"
#include "terminal_input.h"
#include <devices/rawkeycodes.h>
#include <dos/var.h>
#include <proto/codesets.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/keymap.h>
#include <stdlib.h>
#include <string.h>

/* The keymap gives the characters above 127 in the system's 8-bit character
 * set, which Locale prefs save as ENV:CHARSET (ISO-8859-2 for a Polish
 * setup). Their code points come from codesets.library, opened only when a
 * key gives such a character. Without the variable, the library or the set,
 * the bytes are Latin-1, as before. The variable is read again for each such
 * key, so a change in Locale prefs applies without restarting. */
struct Library *CodesetsBase;
static char charset_name[64];
static unsigned short charset_high[128];
static int charset_loaded;

static void close_codesets(void)
{
    if (CodesetsBase) CloseLibrary(CodesetsBase);
    CodesetsBase = NULL;
}

static const unsigned short *keyboard_charset(void)
{
    char name[sizeof charset_name];
    struct codeset *set;
    int i;
    if (GetVar((STRPTR)"CHARSET", (STRPTR)name, sizeof name, GVF_GLOBAL_ONLY) <= 0 || !name[0])
        return NULL;
    if (charset_loaded && !strcmp(name, charset_name))
        return charset_loaded > 0 ? charset_high : NULL;
    strcpy(charset_name, name);
    charset_loaded = -1;
    if (!CodesetsBase) {
        CodesetsBase = OpenLibrary((CONST_STRPTR)"codesets.library", 6);
        if (!CodesetsBase) return NULL;
        atexit(close_codesets);
    }
    set = CodesetsFindA((CONST_STRPTR)name, NULL);
    if (!set) return NULL;
    for (i = 0; i < 128; ++i) {
        unsigned u = set->table[128 + i].ucs4;
        charset_high[i] = u > 0 && u <= 0xffff ? (unsigned short)u : 0;
    }
    charset_loaded = 1;
    return charset_high;
}

static const unsigned short *mapped_charset(const unsigned char *bytes, WORD n)
{
    WORD i;
    for (i = 0; i < n; ++i)
        if (bytes[i] >= 128) return keyboard_charset();
    return NULL;
}

int aros_input_capture(ArosRawKey *key, const struct IntuiMessage *message)
{
    if (!key) return 0;
    memset(key, 0, sizeof *key);
    if (!message || message->Class != IDCMP_RAWKEY ||
        (message->Code & IECODE_UP_PREFIX)) return 0;
    key->event.ie_Class = IECLASS_RAWKEY;
    key->event.ie_Code = message->Code;
    key->event.ie_Qualifier = message->Qualifier;
    /* Intuition stores an APTR-sized copy of the event position union at
     * IAddress. Copy the value, never retain/dereference it as a pointer.
     * APTR-sized copying matters on native x86_64 (not just ULONG). */
    if (message->IAddress)
        memcpy(&key->event.ie_EventAddress, message->IAddress, sizeof(APTR));
    return 1;
}

static TerminalKey special(UWORD code)
{
    switch (code) {
    case RAWKEY_ESCAPE: return TERMINAL_KEY_ESCAPE;
    case RAWKEY_RETURN: case RAWKEY_KP_ENTER: return TERMINAL_KEY_ENTER;
    case RAWKEY_BACKSPACE: return TERMINAL_KEY_BACKSPACE;
    case RAWKEY_TAB: return TERMINAL_KEY_TAB;
    case RAWKEY_UP: return TERMINAL_KEY_UP;
    case RAWKEY_DOWN: return TERMINAL_KEY_DOWN;
    case RAWKEY_LEFT: return TERMINAL_KEY_LEFT;
    case RAWKEY_RIGHT: return TERMINAL_KEY_RIGHT;
    case RAWKEY_HOME: return TERMINAL_KEY_HOME;
    case RAWKEY_END: return TERMINAL_KEY_END;
    case RAWKEY_INSERT: return TERMINAL_KEY_INSERT;
    case RAWKEY_DELETE: return TERMINAL_KEY_DELETE;
    case RAWKEY_PAGEUP: return TERMINAL_KEY_PAGEUP;
    case RAWKEY_PAGEDOWN: return TERMINAL_KEY_PAGEDOWN;
    case RAWKEY_F11: return TERMINAL_KEY_F11;
    case RAWKEY_F12: return TERMINAL_KEY_F12;
    default:
        if (code >= RAWKEY_F1 && code <= RAWKEY_F10)
            return (TerminalKey)(TERMINAL_KEY_F1 + code - RAWKEY_F1);
        return TERMINAL_KEY_NONE;
    }
}

size_t aros_input_encode(const ArosRawKey *key, unsigned char *output,
                         size_t capacity)
{
    struct InputEvent event;
    TerminalKey symbol;
    unsigned modifiers = 0;
    unsigned char mapped[255];
    WORD n;
    if (!key || key->event.ie_Class != IECLASS_RAWKEY ||
        (key->event.ie_Code & IECODE_UP_PREFIX)) return 0;
    event = key->event;
    if (event.ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) modifiers |= TERMINAL_SHIFT;
    if (event.ie_Qualifier & IEQUALIFIER_CONTROL) modifiers |= TERMINAL_CTRL;
    if (event.ie_Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) modifiers |= TERMINAL_ALT;
    symbol = special(event.ie_Code);
    if (symbol != TERMINAL_KEY_NONE)
        return terminal_encode_key(symbol, modifiers, output, capacity);
    if (!(event.ie_Qualifier & IEQUALIFIER_LALT)) modifiers &= ~TERMINAL_ALT;
    event.ie_Qualifier &= ~(IEQUALIFIER_CONTROL | IEQUALIFIER_LALT);
    n = MapRawKey(&event, (STRPTR)mapped, sizeof mapped, NULL);
    if (n <= 0) return 0;
    return terminal_encode_charset(mapped, (size_t)n, mapped_charset(mapped, n),
                                   modifiers, output, capacity);
}
