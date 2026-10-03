#ifndef XTERM_LINK_GESTURE_H
#define XTERM_LINK_GESTURE_H
typedef struct {
    char url[2048];
    int x,y,owned,armed,copy;
} XLinkGesture;
int link_gesture_start(XLinkGesture *,const char *,int,int,int,int);
void link_gesture_motion(XLinkGesture *,int,int);
void link_gesture_cancel(XLinkGesture *);
void link_gesture_new_press(XLinkGesture *);
/* 0 cancelled, 1 open, 2 copy. Always consumes an owned release. */
int link_gesture_finish(XLinkGesture *,const char *,int,int,int,int);
#endif
