#include "../src/line_view.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermCell base_cells[80], copy[80], view_cells[80], hist[2048];
static XTermScreen base, view;
static XLineEditor editor;
static void compose(void)
{
    memcpy(copy,base_cells,sizeof copy);
    assert(line_view_build(&view,&base,&editor,view_cells,80,hist,2048));
    assert(!memcmp(copy,base_cells,sizeof copy));
}
int main(void)
{
    assert(!screen_init(&base,base_cells,80,3,8));
    screen_write(&base,(const unsigned char *)"prompt> ",8);
    assert(line_editor_insert(&editor,"A中B",5)); compose();
    assert(screen_view_cell(&view,0,0).ch=='p');
    assert(screen_view_cell(&view,1,0).ch=='A');
    assert(screen_view_cell(&view,1,1).ch==0x4e2d);
    assert(view.cursor_row==1 && view.cursor_column==4);
    line_editor_action(&editor,XLINE_SELECT_LEFT);
    line_editor_action(&editor,XLINE_SELECT_LEFT); compose();
    assert(screen_view_cell(&view,1,1).bg==4);
    assert(screen_view_cell(&view,1,2).bg==4);
    assert(screen_view_cell(&view,1,3).bg==4);
    assert(view.cursor_column==1);
    line_editor_action(&editor,XLINE_BACKSPACE); compose();
    assert(screen_view_cell(&view,1,0).ch=='A');
    assert(screen_view_cell(&view,1,1).ch==' ');
    line_editor_action(&editor,XLINE_CLEAR);
    assert(line_editor_insert(&editor,"0123456789012345678901234567890123456789",40));
    line_editor_action(&editor,XLINE_HOME); compose();
    assert(view.cursor_row<view.rows && view.view_offset>0);
    assert(screen_view_cell(&view,view.cursor_row,0).ch=='p' ||
           screen_view_cell(&view,view.cursor_row,0).ch=='0');
    line_editor_action(&editor,XLINE_END); compose();
    assert(view.cursor_row<view.rows && !view.view_offset);
    assert(!screen_resize(&base,2,5)); compose();
    assert(view.columns==5 && view.cursor_row<2);
    line_editor_action(&editor,XLINE_CLEAR);
    assert(line_editor_insert(&editor,"❤️",strlen("❤️"))); compose();
    assert(editor.cursor==strlen("❤️"));
    assert(!line_view_build(&view,&base,&editor,view_cells,1,hist,2048));
    puts("line view PASS: prompt, wrap, Unicode selection, shrinking, long lines, resize, isolation");
    return 0;
}
