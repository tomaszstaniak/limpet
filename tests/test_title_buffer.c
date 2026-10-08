/* The window title Intuition holds must never be written: the title bar is
 * drawn in input.device's task, which may be reading it at that moment.
 * A new title is composed in the other buffer and only then handed over. */
#include "../src/title_buffer.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    XTitleBuffers t;
    char *next;
    const char *shown;
    title_buffers_init(&t);
    assert(title_buffers_shown(&t)[0]==0);

    /* Composing never touches the shown buffer. */
    next=title_buffers_next(&t);
    assert(next!=title_buffers_shown(&t));
    strcpy(next,"Limpet");
    assert(title_buffers_commit(&t));
    shown=title_buffers_shown(&t);
    assert(shown==next && !strcmp(shown,"Limpet"));

    next=title_buffers_next(&t);
    assert(next!=shown);
    strcpy(next,"Limpet | Selected | RAmiga+C or Ctrl+Shift+C");
    assert(!strcmp(shown,"Limpet"));            /* still intact while composing */
    assert(title_buffers_commit(&t));
    assert(title_buffers_shown(&t)==next);
    assert(!strcmp(shown,"Limpet"));            /* the old one is left as it was */

    /* The same text again: nothing to hand over, the shown buffer stays. */
    shown=title_buffers_shown(&t);
    next=title_buffers_next(&t);
    strcpy(next,"Limpet | Selected | RAmiga+C or Ctrl+Shift+C");
    assert(!title_buffers_commit(&t));
    assert(title_buffers_shown(&t)==shown);

    /* Back and forth keeps alternating between the two buffers. */
    next=title_buffers_next(&t);
    strcpy(next,"Limpet");
    assert(title_buffers_commit(&t));
    assert(title_buffers_shown(&t)!=shown);
    assert(title_buffers_next(&t)==shown);
    assert(sizeof t.text[0]>=160);
    return 0;
}
