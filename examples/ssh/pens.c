/* Kept apart from stream.c: the SDK header and the terminal's own headers
 * declare the same control block. */
#include "pens.h"
#include <limpet/limpet.h>

int xt_ssh_output_uses_pens(BPTR output)
{
    LimpetPresentation presentation;
    if (limpet_query_presentation(output, &presentation, NULL) != LIMPET_OK) return 0;
    return limpet_uses_pens(&presentation);
}
