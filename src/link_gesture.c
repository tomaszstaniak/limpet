#include "link_gesture.h"
#include <string.h>

int link_gesture_start(XLinkGesture *g,const char *url,int x,int y,int ctrl,int shift)
{
    size_t n;
    g->owned=g->armed=0;
    if(!ctrl || !url) return 0;
    for(n=0;n<sizeof g->url && url[n];++n) {}
    if(!n || n==sizeof g->url) return 0;
    memcpy(g->url,url,n+1);
    g->x=x; g->y=y; g->copy=!!shift;
    g->owned=g->armed=1;
    return 1;
}
void link_gesture_cancel(XLinkGesture *g) { g->armed=0; }
void link_gesture_new_press(XLinkGesture *g) { g->owned=g->armed=0; }
void link_gesture_motion(XLinkGesture *g,int x,int y)
{
    /* Compare in a wider type: coordinates originate in signed WORDs. */
    long dx=(long)x-g->x,dy=(long)y-g->y;
    if(dx < -3 || dx > 3 || dy < -3 || dy > 3) g->armed=0;
}
int link_gesture_finish(XLinkGesture *g,const char *url,int x,int y,int ctrl,int shift)
{
    int action;
    link_gesture_motion(g,x,y);
    action=g->owned && g->armed && ctrl && !!shift==g->copy &&
        url && !strcmp(g->url,url) ? (g->copy?2:1) : 0;
    g->owned=g->armed=0;
    return action;
}
