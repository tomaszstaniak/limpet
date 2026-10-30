#include "window_alpha.h"
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/clip.h>
#include <graphics/layers.h>

#ifndef SA_SourceAlphaSupport
#define SA_SourceAlphaSupport (SA_Dummy + 132)
#endif
#ifndef WA_AlphaSurface
#define WA_AlphaSurface (WA_Dummy + 105)
#endif
#ifndef LAYERCOMPOSITED
#define LAYERCOMPOSITED (1<<3)
#endif

int window_alpha_supported(struct Screen *screen)
{
    IPTR alpha = 0;
    if (!screen) return 0;
    if (!GetAttr(SA_SourceAlphaSupport, (Object *)screen, &alpha)) return 0;
    return alpha != 0;
}

unsigned long window_alpha_tag(void) { return WA_AlphaSurface; }

int window_alpha_composited(const struct Window *window)
{
    return window && window->WLayer && (window->WLayer->Flags & LAYERCOMPOSITED) != 0;
}
