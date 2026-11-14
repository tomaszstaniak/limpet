#include "window_alpha.h"
#include "background.h"
#include <proto/exec.h>
#include <proto/intuition.h>
#include <exec/libraries.h>
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
    if (!GetAttr(SA_SourceAlphaSupport, (Object *)screen, &alpha) || !alpha) return 0;
    /* The screen says yes; the frame of such a window needs more
       (background.h). layers.library is in the ROM: no file is read. */
    {
        struct Library *layers = OpenLibrary((CONST_STRPTR)"layers.library", 0);
        int ok = layers && background_layers_draw_frame(layers->lib_Version, layers->lib_Revision);
        if (layers) CloseLibrary(layers);
        return ok;
    }
}

unsigned long window_alpha_tag(void) { return WA_AlphaSurface; }

int window_alpha_composited(const struct Window *window)
{
    return window && window->WLayer && (window->WLayer->Flags & LAYERCOMPOSITED) != 0;
}
