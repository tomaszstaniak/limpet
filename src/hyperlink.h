#ifndef AROS_XTERM_HYPERLINK_H
#define AROS_XTERM_HYPERLINK_H
#include <stddef.h>
#include <stdint.h>
#define XHYPERLINK_LIMIT 2048
#define XHYPERLINK_ENTRIES 64
#define XHYPERLINK_INVALID UINT32_MAX
struct XTermScreen;
typedef struct XHyperlinks {
    struct { uint32_t token; char url[XHYPERLINK_LIMIT]; } entries[XHYPERLINK_ENTRIES];
    uint32_t next_token;
    unsigned next_slot;
} XHyperlinks;
typedef struct XHyperlinkHit {
    char url[XHYPERLINK_LIMIT];
    uint32_t token;
    size_t first, last;
    unsigned generation;
} XHyperlinkHit;
/* Initialize only with a full reset of every screen/history that uses this
 * store. Tokens are never reused during its lifetime, including exhaustion. */
void hyperlinks_init(XHyperlinks *);
/* Attaches caller-owned initialized storage; does not initialize the store. */
void screen_attach_links(struct XTermScreen *, XHyperlinks *);
int hyperlink_at(const struct XTermScreen *, unsigned, unsigned, XHyperlinkHit *);
int hyperlink_contains(const struct XTermScreen *, const XHyperlinkHit *, unsigned, unsigned);
/* Strict launch policy. HTTP targets retain percent triplets for OpenURL.
 * Local file targets decode to a DOS path for Workbench without shell parsing. */
int hyperlink_open_safe(const char *);
/* Local file:///Volume:path only; percent decoding never invokes a shell.
 * Remote authorities, control bytes and relative/Unix paths are rejected. */
int hyperlink_file_path(const char *,char *,size_t);
/* Parser interface: rejected explicit targets receive a nonzero sentinel. */
uint32_t hyperlink_register(XHyperlinks *, const char *);
#endif
