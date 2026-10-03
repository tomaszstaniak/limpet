/* Native integration probe: run with the standard US/default guest keymap. */
#include "../src/aros_input.h"
#include "../src/echo_session.h"
#include <devices/rawkeycodes.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct IntuiMessage message;
    struct InputEvent previous;
    ArosRawKey key;
    unsigned char out[AROS_INPUT_MAX_BYTES];
    EchoSession session;
    size_t n;
    memset(&message, 0, sizeof message);
    memset(&previous, 0, sizeof previous);
    previous.ie_Prev1DownCode = 0x2a;
    previous.ie_Prev1DownQual = IEQUALIFIER_RALT;
    previous.ie_Prev2DownCode = 0x1a;
    previous.ie_Prev2DownQual = IEQUALIFIER_LSHIFT;
    message.Class = IDCMP_RAWKEY;
    message.Code = RAWKEY_LEFT;
    message.Qualifier = IEQUALIFIER_CONTROL;
    message.IAddress = &previous.ie_EventAddress;
    assert(aros_input_capture(&key, &message));
    memset(&previous, 0, sizeof previous); /* Model message storage reuse. */
    assert(key.event.ie_Prev1DownCode == 0x2a);
    assert(key.event.ie_Prev1DownQual == IEQUALIFIER_RALT);
    assert(key.event.ie_Prev2DownCode == 0x1a);
    assert(key.event.ie_Prev2DownQual == IEQUALIFIER_LSHIFT);
    assert(aros_input_encode(&key, out, sizeof out) == 6);
    assert(memcmp(out, "\033[1;5D", 6) == 0);
    message.Code |= IECODE_UP_PREFIX;
    assert(!aros_input_capture(&key, &message));
    assert(aros_input_encode(&key, out, sizeof out) == 0);
    message.Code = RAWKEY_A;
    message.Qualifier = 0;
    message.IAddress = NULL;
    assert(aros_input_capture(&key, &message));
    n = aros_input_encode(&key, out, sizeof out);
    assert(n == 1 && out[0] == 'a');
    echo_session_init(&session);
    assert(echo_session_write(&session, out, n) == n);
    assert(echo_session_read(&session, out, sizeof out) == 1 && out[0] == 'a');
    message.Qualifier = IEQUALIFIER_LSHIFT;
    assert(aros_input_capture(&key, &message));
    assert(aros_input_encode(&key, out, sizeof out) == 1 && out[0] == 'A');
    message.Code = RAWKEY_C;
    message.Qualifier = IEQUALIFIER_CONTROL | IEQUALIFIER_LALT;
    assert(aros_input_capture(&key, &message));
    assert(aros_input_encode(&key, out, sizeof out) == 2);
    assert(memcmp(out, "\033\003", 2) == 0);
    message.Code = RAWKEY_SPACE;
    message.Qualifier = IEQUALIFIER_CONTROL;
    assert(aros_input_capture(&key, &message));
    assert(aros_input_encode(&key, out, sizeof out) == 1 && out[0] == 0);
    puts("native input capture/keymap/echo tests passed (US keymap)");
    return 0;
}
