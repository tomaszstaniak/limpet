#include "../src/link_gesture.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    XLinkGesture g={0};
    assert(!link_gesture_start(&g,"https://aros.org",10,20,0,0));
    assert(link_gesture_start(&g,"https://aros.org",10,20,1,0));
    assert(link_gesture_finish(&g,"https://aros.org",10,20,1,0)==1);
    assert(!g.owned);
    assert(!link_gesture_finish(&g,"https://aros.org",10,20,1,0));
    assert(link_gesture_start(&g,"https://aros.org",10,20,1,1));
    assert(link_gesture_finish(&g,"https://aros.org",11,21,1,1)==2);
    link_gesture_start(&g,"https://aros.org",10,20,1,0);
    link_gesture_motion(&g,30,20);
    assert(!link_gesture_finish(&g,"https://aros.org",10,20,1,0));
    link_gesture_start(&g,"https://aros.org",10,20,1,0);
    link_gesture_cancel(&g);
    assert(g.owned); /* Consume release even after output/focus invalidation. */
    assert(!link_gesture_finish(&g,"https://aros.org",10,20,1,0));
    link_gesture_start(&g,"https://aros.org",10,20,1,0);
    assert(!link_gesture_finish(&g,"https://example.org",10,20,1,0));
    link_gesture_start(&g,"https://aros.org",10,20,1,0);
    assert(!link_gesture_finish(&g,"https://aros.org",10,20,0,0));
    link_gesture_start(&g,"https://aros.org",10,20,1,0);
    assert(!link_gesture_finish(&g,"https://aros.org",10,20,1,1));
    assert(!link_gesture_start(&g,NULL,10,20,1,0));
    link_gesture_start(&g,"https://aros.org",10,20,1,0);
    link_gesture_cancel(&g); /* Focus loss, release outside this window. */
    link_gesture_new_press(&g);
    assert(!g.owned && !g.armed); /* Next plain drag must belong to selection. */
    puts("link gesture: PASS");
    return 0;
}
