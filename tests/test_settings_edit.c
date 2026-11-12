#include "../src/settings_edit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char out[XSETTINGS_FILE_MAX + 1];
static size_t n;
static int edit(const char *old, const char *conf)
{
    XSettings s; const char *why;
    settings_default(&s);
    assert(!settings_parse_session(&s, conf, strlen(conf), &why));
    return settings_edit_session(old, strlen(old), &s, out, sizeof out, &n);
}
int main(void)
{
    XSettings back; const char *why;
    /* Window.conf: its own keys only, defaults left out, other lines kept. */
    {
        XSettings w;
        settings_default(&w);
        assert(w.background_opacity == 100 && !w.background_blur);
        assert(!settings_parse_window(&w, "background_opacity=70\nbackground_blur=on\n", 41, &why));
        assert(w.background_opacity == 70 && w.background_blur == 1);
        assert(settings_edit_window("# mine\nbackground_opacity=30\n", 29, &w, out, sizeof out, &n));
        assert(!strcmp(out, "# mine\nbackground_opacity=70\nbackground_blur=on\n"));
        settings_default(&w);
        assert(settings_edit_window("# mine\nbackground_opacity=30\nbackground_blur=on\n", 48, &w, out, sizeof out, &n));
        assert(!strcmp(out, "# mine\n"));
        settings_default(&w);
        /* Out of range, unknown words and other files' keys: the whole file is refused. */
        assert(settings_parse_window(&w, "background_opacity=19\n", 22, &why) == 1);
        assert(settings_parse_window(&w, "background_opacity=101\n", 23, &why) == 1);
        assert(settings_parse_window(&w, "background_blur=yes\n", 20, &why) == 1);
        assert(settings_parse_window(&w, "font_size=12\n", 13, &why) == 1);
        assert(w.background_opacity == 100);
        /* And its keys are unknown to the other two files. */
        assert(settings_parse(&w, "background_opacity=70\n", 22, &why) == 1);
        assert(settings_parse_session(&w, "background_blur=on\n", 19, &why) == 1);
    }
    /* Nothing chosen: the file comes back as it was. */
    assert(edit("", "") && !n && !out[0]);
    assert(edit("# mine\nx = y\n", "") && !strcmp(out, "# mine\nx = y\n"));
    /* Back to nothing from a file of session keys only: an empty file, not an error. */
    assert(edit("session_start=fastfetch\nfastfetch_look=picture\n", "") && !n && !out[0]);
    /* Other lines kept in place, the session keys where they were, new ones at the end. */
    assert(edit("# Limpet\r\nfont=A.ttf\r\nsession_start = fastfetch\r\ntheme=B.theme\r\n",
                "session_start=command\nsession_command=Echo hi\n"));
    assert(!strcmp(out, "# Limpet\r\nfont=A.ttf\r\nsession_start=command\ntheme=B.theme\r\nsession_command=Echo hi\n"));
    /* Turned off: the keys at their defaults go; a line Limpet does not know stays. */
    assert(edit("x-unknown=1\nsession_start=fastfetch\nfastfetch_look=picture\nhistory=50", ""));
    assert(!strcmp(out, "x-unknown=1\nhistory=50\n"));
    /* Duplicates of a session key are not kept twice. */
    assert(edit("session_start=off\nsession_start=fastfetch\n", "session_start=fastfetch"));
    assert(!strcmp(out, "session_start=fastfetch\n"));
    /* Everything, and it reads back the same. */
    assert(edit("# kept\n", "session_start=fastfetch\nfastfetch=Work:FF/fastfetch\nfastfetch_look=file\n"
                "fastfetch_config=RAM:a b.jsonc\nfastfetch_hide=network,system\nfastfetch_logo=text\n"));
    settings_default(&back);
    assert(!strncmp(out, "# kept\n", 7));
    assert(!settings_parse_session(&back, out + 7, strlen(out) - 7, &why));
    assert(back.session_start == XSTART_FASTFETCH && !strcmp(back.fastfetch, "Work:FF/fastfetch"));
    assert(back.fastfetch_look == XFF_FILE && !strcmp(back.fastfetch_config, "RAM:a b.jsonc"));
    assert(back.fastfetch_hide == (XFF_SYSTEM | XFF_NETWORK) && back.fastfetch_logo == XFF_LOGO_TEXT);
    assert(strstr(out, "fastfetch_hide=system,network\n"));
    /* Too large for a settings file: refused, not cut. */
    {   static char big[XSETTINGS_FILE_MAX]; XSettings s;
        memset(big, '#', sizeof big - 10); big[sizeof big - 10] = '\n'; big[sizeof big - 9] = 0;
        settings_default(&s); s.session_start = XSTART_COMMAND; strcpy(s.session_command, "Echo a long enough line");
        assert(!settings_edit_session(big, strlen(big), &s, out, sizeof out, &n)); }
    /* Limpet.conf: the user's own lines stay where they are. */
    {
        static const char old[]="# my settings\nfont=FONTS:TrueType/DejaVuSansMono.ttf\n\n# colours\ntheme=Old\nscrollback=500\n";
        XSettings m, back;
        const char *why=NULL;
        char text[XSETTINGS_FILE_MAX+1];
        size_t length;
        settings_default(&m);
        assert(!settings_parse(&m,old,strlen(old),&why));
        strcpy(m.theme,"Dracula"); m.font_size=15; m.scrollback=XSETTINGS_SCROLLBACK_DEFAULT;
        assert(settings_edit_main(old,strlen(old),&m,text,sizeof text,&length));
        assert(!strcmp(text,"# my settings\nfont=FONTS:TrueType/DejaVuSansMono.ttf\n\n# colours\ntheme=Dracula\nfont_size=15\n"));
        settings_default(&back);
        assert(!settings_parse(&back,text,length,&why));
        assert(!strcmp(back.theme,"Dracula") && back.font_size==15 && back.scrollback==XSETTINGS_SCROLLBACK_DEFAULT);
        /* No theme chosen: the built-in one, the line goes. A path with spaces stays whole. */
        m.theme[0]=0; strcpy(m.font,"Work:My Fonts/Mono.ttf");
        assert(settings_edit_main(text,length,&m,text+2048,2048,&length));
        assert(!strcmp(text+2048,"# my settings\nfont=Work:My Fonts/Mono.ttf\n\n# colours\nfont_size=15\n"));
        /* From nothing; and nothing chosen leaves an empty file. */
        settings_default(&m); strcpy(m.theme,"magicwb"); m.history=100;
        assert(settings_edit_main("",0,&m,text,sizeof text,&length));
        assert(!strcmp(text,"theme=magicwb\nhistory=100\n"));
        settings_default(&m);
        assert(settings_edit_main("theme=x\n",8,&m,text,sizeof text,&length) && !length);
        /* The session and window keys are not this file's: left alone as unknown lines. */
        assert(settings_edit_main("bold_bright=off\ntheme=x\n",24,&m,text,sizeof text,&length));
        assert(!strcmp(text,"bold_bright=off\n"));
        /* Colours.conf. */
        settings_default(&m);
        assert(settings_edit_colours("# c\n",4,&m,text,sizeof text,&length) && !strcmp(text,"# c\n"));
        m.bold_bright=0;
        assert(settings_edit_colours("# c\n",4,&m,text,sizeof text,&length) && !strcmp(text,"# c\nbold_bright=off\n"));
        settings_default(&back);
        assert(!settings_parse_colours(&back,text,length,&why) && back.bold_bright==0);
        m.bold_bright=1;
        assert(settings_edit_colours(text,length,&m,text+2048,2048,&length) && !strcmp(text+2048,"# c\n"));
    }
    puts("settings edit PASS: other lines kept in place, keys replaced or appended, defaults left out, round trip, size");
    return 0;
}
