#ifndef XTERM_CLUSTER_FONT_H
#define XTERM_CLUSTER_FONT_H
#include <stdint.h>
#define XCLUSTER_MAX 16
#define XRASTER_MAX 32
typedef struct XClusterRaster {
    /* Premultiplied ARGB. MONO/GRAY faces store white coverage, tinted at draw. */
    uint32_t pixels[XRASTER_MAX * XRASTER_MAX];
    unsigned width, height, source, glyphs, colour, status;
} XClusterRaster;
const XClusterRaster *xfont_cluster(const uint32_t *, unsigned count, unsigned span);
void xfont_cluster_rgb(const XClusterRaster *, uint32_t fg, uint32_t bg,
                      unsigned char *rgb);
#endif
