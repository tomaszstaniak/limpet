#include "echo_session.h"
void echo_session_init(EchoSession *session)
{
    if (!session) return;
    session->head = session->length = 0;
    session->open = 1;
}
void echo_session_close(EchoSession *session)
{
    if (!session) return;
    session->head = session->length = 0;
    session->open = 0;
}
static void put(EchoSession *session, unsigned char c)
{
    session->bytes[(session->head + session->length) % ECHO_SESSION_CAPACITY] = c;
    ++session->length;
}
size_t echo_session_write(EchoSession *session, const unsigned char *input,
                          size_t length)
{
    size_t consumed = 0;
    if (!session || !session->open || !input) return 0;
    while (consumed < length) {
        unsigned char c = input[consumed];
        size_t n = c == '\r' ? 2 : (c == '\b' || c == 127 ? 3 : 1);
        if (n > ECHO_SESSION_CAPACITY - session->length) break;
        if (c == '\r') { put(session, '\r'); put(session, '\n'); }
        else if (c == '\b' || c == 127) {
            put(session, '\b'); put(session, ' '); put(session, '\b');
        } else put(session, c);
        ++consumed;
    }
    return consumed;
}
size_t echo_session_read(EchoSession *session, unsigned char *output,
                         size_t capacity)
{
    size_t n = 0;
    if (!session || !session->open || !output) return 0;
    while (n < capacity && session->length) {
        output[n++] = session->bytes[session->head];
        session->head = (session->head + 1) % ECHO_SESSION_CAPACITY;
        --session->length;
    }
    return n;
}
