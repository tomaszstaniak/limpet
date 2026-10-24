#ifndef LIMPET_CONSOLE_GEOMETRY_H
#define LIMPET_CONSOLE_GEOMETRY_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* PR1763, 4fd9618699101fa6425146d7d0c0eb7767fe23cf. Prefer the public
 * SDK definition when installed; older mainline and ABIv11 SDKs lack it. */
#if defined(__has_include)
# if __has_include(<dos/consoleinfo.h>)
#  include <dos/consoleinfo.h>
# endif
#endif
#ifndef ACTION_CONSOLE_GETGEOMETRY
#define ACTION_CONSOLE_GETGEOMETRY 0x434f4e47L
#define CONSOLE_GEOMETRY_VERSION 1
struct ConsoleGeometry {
    uint32_t cg_Size,cg_Version,cg_Columns,cg_Rows,cg_Width,cg_Height;
};
#endif
typedef char LimpetConsoleGeometrySize[(sizeof(struct ConsoleGeometry)==24)?1:-1];
static inline int console_geometry_reply(void *block,ptrdiff_t capacity,
        uintptr_t version,unsigned columns,unsigned rows,unsigned width,unsigned height)
{
    struct ConsoleGeometry out;
    if(!block || capacity<(ptrdiff_t)sizeof out || version!=CONSOLE_GEOMETRY_VERSION)
        return 0;
    out.cg_Size=sizeof out;out.cg_Version=CONSOLE_GEOMETRY_VERSION;
    out.cg_Columns=columns;out.cg_Rows=rows;
    out.cg_Width=width;out.cg_Height=height;
    memcpy(block,&out,sizeof out);return 1;
}
#endif
