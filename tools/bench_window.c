/* Diagnostic only: exercise the production renderer including pixel upload.
 * This is not a keyboard/IPC latency benchmark. */
#define main xterm_preview_main
#include "../src/aros_xterm.c"
#undef main
#include <stdlib.h>

static unsigned long ticks(void)
{
    struct DateStamp t;
    DateStamp(&t);
    return ((unsigned long)t.ds_Days*24*60+t.ds_Minute)*60*TICKS_PER_SECOND+t.ds_Tick;
}
static int capture(struct Window *w,unsigned char *pixels)
{
    unsigned width=screen.columns*xfont_width(),height=screen.rows*xfont_height();
    return ReadPixelArray(pixels,0,0,width*3,w->RPort,MARGIN,MARGIN,width,height,
                          RECTFMT_RGB)==width*height;
}
static int compare_full(struct Window *w,unsigned char *pixels,unsigned char *reference)
{
    size_t bytes=(size_t)screen.rows*screen.columns*xfont_width()*xfont_height()*3;
    draw_screen(w);
    if(!capture(w,pixels)) return 0;
    paint_damage_invalidate(&paint_damage);
    draw_screen(w);
    return capture(w,reference) && !memcmp(pixels,reference,bytes);
}
int main(void)
{
    struct Window *w;
    unsigned char *pixels,*reference;
    size_t bytes;
    unsigned i; int ok=1;
    unsigned long start,elapsed,before;
    theme_default(&theme);
    paint_damage_init(&paint_damage,paint_cache,MAX_ROWS*MAX_COLUMNS);
    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,24,80);
    replay();
    if(xfont_open()) return 20;
    w=OpenWindowTags(NULL,WA_Title,(IPTR)"XTerm renderer acceptance",
        WA_Left,40,WA_Top,60,WA_InnerWidth,80*xfont_width()+2*MARGIN,
        WA_InnerHeight,24*xfont_height()+2*MARGIN,
        WA_Flags,WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_GIMMEZEROZERO|WFLG_SIMPLE_REFRESH,
        TAG_DONE);
    if(!w) { xfont_close(); return 20; }
    bytes=(size_t)24*80*xfont_width()*xfont_height()*3;
    pixels=malloc(bytes); reference=malloc(bytes);
    if(!pixels || !reference) ok=0;
    if(ok) {
        draw_screen(w); before=paint_rows;
        for(i=0;i<100;++i) draw_screen(w);
        printf("unchanged: 100 draws, %lu submitted rows (expected 0)\n",paint_rows-before);
        if(paint_rows!=before) ok=0;
        for(i=0;i<40 && ok;++i) {
            char text[80];
            snprintf(text,sizeof text,"\033[24;1Htyping %02u é 中 😀\033[K",i);
            write_text(text);
            if(i%4==0) screen.cursor_visible=!screen.cursor_visible;
            if(i%4==1) { selection_start(&selection,&screen,23,0); selection_extend(&selection,&screen,23,16); }
            if(i%4==2) selection.active=selection.dragging=0;
            if(i%4==3) { screen_scroll_view(&screen,1); }
            ok=compare_full(w,pixels,reference);
            screen_scroll_view(&screen,-(int)screen.view_offset);
        }
        printf("damage/full pixel comparison: %s (%u transitions)\n",ok?"PASS":"FAIL",i);
        if(ok) {
            selection.active=selection.dragging=0;
            screen_scroll_view(&screen,-(int)screen.view_offset);
            write_text("\033[2J\033[Hhttps://aros.org\r\n");
            link_hovered=hyperlink_at(&screen,0,2,&hovered_link);
            ok=link_hovered && compare_full(w,pixels,reference);
            link_hovered=0;
            if(ok) ok=compare_full(w,pixels,reference);
            if(ok) { screen_resize(&screen,22,72); ok=compare_full(w,pixels,reference); }
            if(ok) { screen_resize(&screen,24,80); ok=compare_full(w,pixels,reference); }
            printf("hover and model-resize pixel comparison: %s\n",ok?"PASS":"FAIL");
        }
        selection.active=selection.dragging=0;
        screen.cursor_visible=1;
        for(i=0;i<4 && ok;++i) {
            unsigned j; int full=i==1 || i==2;
            replay(); draw_screen(w); start=ticks(); before=paint_rows;
            for(j=0;j<300;++j) {
                char text[80];
                snprintf(text,sizeof text,"\033[24;1Htyping %03u é 中 😀\033[K",j);
                write_text(text);
                if(full) paint_damage_invalidate(&paint_damage);
                draw_screen(w);
            }
            elapsed=ticks()-start;
            printf("typing %s: 300 draws, %lu rows, %lu ticks at %u Hz\n",
                full?"forced-full":"damage",paint_rows-before,elapsed,(unsigned)TICKS_PER_SECOND);
        }
        for(i=0;i<4 && ok;++i) {
            unsigned j; int full=i==1 || i==2;
            replay(); draw_screen(w); start=ticks(); before=paint_rows;
            for(j=0;j<300;++j) {
                char text[80];
                snprintf(text,sizeof text,"\r\nscroll %03u é 中 😀",j);
                write_text(text);
                if(full) paint_damage_invalidate(&paint_damage);
                draw_screen(w);
            }
            elapsed=ticks()-start;
            printf("scroll %s: 300 draws, %lu rows, %lu ticks at %u Hz\n",
                full?"forced-full":"damage",paint_rows-before,elapsed,(unsigned)TICKS_PER_SECOND);
        }
    }
    free(pixels); free(reference);
    CloseWindow(w); xfont_close();
    printf("window renderer acceptance: %s\n",ok?"PASS":"FAIL");
    return ok?0:20;
}
