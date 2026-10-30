#ifndef XTERM_WINDOW_ALPHA_H
#define XTERM_WINDOW_ALPHA_H
/* Windows with per pixel alpha, asked of the running system and never
 * assumed from the SDK, the ABI or a library version. SDKs without the
 * compositor's definitions build the same program: the tags are constants,
 * and a system that does not know them answers no. */
struct Screen;
struct Window;
/* The screen composes windows with per pixel alpha (SA_SourceAlphaSupport).
 * Screens that cannot: older mainline, ABIv11, 15/16/24-bit modes. */
int window_alpha_supported(struct Screen *);
/* The tag asking for such a window, for OpenWindow; 0 when it is not wanted. */
unsigned long window_alpha_tag(void);
/* The window has its surface now. To be asked after opening and after each
 * size change: the system turns a surface that cannot grow into an ordinary
 * window without notice (graphics/layersext.h). */
int window_alpha_composited(const struct Window *);
#endif
