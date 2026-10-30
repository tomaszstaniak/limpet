#include "backdrop_blur.h"
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>

/* Dockbar's present_compose.c; not in any SDK. */
#define PROPOSED_WA_BackdropBlur (WA_Dummy + 107)
#define PROPOSED_SA_BackdropBlurSupport (SA_Dummy + 161)
/* Dockbar's glass: the radius looked at on real windows. */
#define BLUR_RADIUS 5

int backdrop_blur_supported(struct Screen *screen)
{
    IPTR blur = 0;
    if (!screen) return 0;
    if (!GetAttr(PROPOSED_SA_BackdropBlurSupport, (Object *)screen, &blur)) return 0;
    return blur != 0;
}

void backdrop_blur_tag(struct TagItem *tag, int wanted)
{
    tag->ti_Tag = wanted ? PROPOSED_WA_BackdropBlur : TAG_IGNORE;
    tag->ti_Data = wanted ? BLUR_RADIUS : 0;
}
