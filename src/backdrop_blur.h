#ifndef XTERM_BACKDROP_BLUR_H
#define XTERM_BACKDROP_BLUR_H
/* Blur of what lies behind a translucent window, done by the compositor:
 * Limpet only asks for it. This is the one place that knows the contract,
 * a proposal for mainline from the Dockbar project (AROS issue #1981) that
 * no AROS release has. Its numbers are working numbers and may change or
 * go; only this file then changes. Without an answer from the running
 * screen no blur tag is sent, and everything works as if it did not exist. */
struct Screen;
struct TagItem;
int backdrop_blur_supported(struct Screen *);
/* Fills a tag for OpenWindow: the request, or TAG_IGNORE. */
void backdrop_blur_tag(struct TagItem *, int wanted);
#endif
