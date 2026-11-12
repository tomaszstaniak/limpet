/* Colour files of other terminals: Ghostty and Kitty line formats, what is
 * recognised and what is refused, and where a theme's name is looked for. */
#include "../src/theme_file.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XThemeReport r;
static int parse(XTheme *t,const char *text)
{ theme_default(t); return theme_file_parse(t,text,strlen(text),&r); }
static int refused(const char *text,int error)
{
    XTheme t,saved;
    theme_default(&t); t.foreground=0x123456; saved=t;
    if(theme_file_parse(&t,text,strlen(text),&r)!=-1) return 0;
    return r.error==error && !memcmp(&t,&saved,sizeof t);
}
static void message(const char *name,const char *instead,const char *expected)
{
    char text[200];
    theme_file_message(text,sizeof text,name,&r,instead);
    if(strcmp(text,expected)) { printf("message: got '%s'\n  expected '%s'\n",text,expected); abort(); }
}

static void same_scheme_three_ways(void)
{
    XTheme native,ghostty,kitty;
    theme_default(&native); theme_default(&ghostty); theme_default(&kitty);
    assert(!theme_file_load(&native,"tests/themes/sample.theme",&r));
    assert(r.format==XTHEME_FORMAT_NATIVE && !r.ignored);
    assert(!theme_file_load(&ghostty,"tests/themes/sample.ghostty",&r));
    assert(r.format==XTHEME_FORMAT_LINES && !r.ignored);
    assert(!theme_file_load(&kitty,"tests/themes/sample.kitty.conf",&r));
    assert(r.format==XTHEME_FORMAT_LINES && !r.ignored);
    assert(native.ansi[0]==0x101112 && native.ansi[9]==0xf04050 && native.ansi[15]==0xf8f9fa);
    assert(native.background==0x181a1c && native.foreground==0xe0e2e4 && native.cursor==0xffcc00);
    assert(native.selection_bg==0x304060 && native.selection_fg==0xffffff);
    assert(!memcmp(&native,&ghostty,sizeof native));
    assert(!memcmp(&native,&kitty,sizeof native));
    message("sample",".",""); /* a clean load says nothing */
    puts("theme file PASS: one scheme in Limpet's, Ghostty's and Kitty's format gives one theme");
}

static void variants(void)
{
    XTheme t;
    /* Ghostty: no blanks, hex without '#', 0x index, quotes, #rgb. */
    assert(!parse(&t,"palette=0x1=AA0000\npalette = 2 = \"#00bb00\"\nbackground=000000\nforeground = #fff\n"));
    assert(t.ansi[1]==0xaa0000 && t.ansi[2]==0x00bb00 && t.background==0x000000 && t.foreground==0xffffff);
    /* Bright half copied from the normal one where only that was given. */
    assert(t.ansi[9]==0xaa0000 && t.ansi[10]==0x00bb00);
    /* Kitty: tabs, CRLF, none, the keyword background, colour above 15. */
    assert(!parse(&t,"foreground\t#D0D0D0\r\nbackground\t#101010\r\ncursor none\r\n"
                    "cursor_text_color background\r\nselection_foreground none\r\n"
                    "selection_background #404040\r\ncolor255 #123456\r\n"));
    assert(t.foreground==0xd0d0d0 && t.background==0x101010);
    assert(t.cursor==0xd0d0d0);                 /* none: from the foreground */
    assert(t.selection_bg==0x404040 && t.selection_fg==0xd0d0d0);
    assert(r.ignored==1);                       /* color255 */
    /* A byte order mark, comments, blank lines, a later value wins. */
    assert(!parse(&t,"\xef\xbb\xbf# comment\n\n  background = #111111\nforeground = #eeeeee\nbackground = #222222\n"));
    assert(t.background==0x222222);
    /* Ghostty's cell-foreground: the role is left to be derived. */
    assert(!parse(&t,"background = #000000\nforeground = #ffffff\nselection-foreground = cell-foreground\n"
                    "selection-background = #333333\n"));
    assert(t.selection_fg==0xffffff);
    /* Shared keys alone, as in a minimal file: still a theme. */
    assert(!parse(&t,"foreground=#101010\nbackground=#f0f0f0\n"));
    assert(r.format==XTHEME_FORMAT_LINES && theme_luma(t.ansi[1])<200);
    puts("theme file PASS: Ghostty and Kitty spellings, byte order mark, CRLF, none, later value wins");
}

static void whole_configuration(void)
{
    /* A user's whole kitty.conf: hundreds of settings, a few colours. */
    static char text[20000];
    XTheme t;
    size_t n=0;
    int i;
    n+=(size_t)snprintf(text+n,sizeof text-n,"font_family JetBrains Mono\nfont_size 11.0\ninclude other.conf\n");
    for(i=0;i<300;++i) n+=(size_t)snprintf(text+n,sizeof text-n,"map ctrl+shift+f%d launch --type=tab\n",i);
    n+=(size_t)snprintf(text+n,sizeof text-n,"background #1e1e2e\nforeground #cdd6f4\ncolor1 #f38ba8\n"
                        "background_opacity 0.9\nconfig-file ~/secret\n");
    theme_default(&t);
    assert(!theme_file_parse(&t,text,n,&r));
    assert(t.background==0x1e1e2e && t.foreground==0xcdd6f4 && t.ansi[1]==0xf38ba8);
    assert(r.ignored==305);
    message("kitty.conf","built-in dark theme","Theme kitty.conf: 305 settings ignored");
    /* A line longer than 255 bytes that is not a colour is one ignored line. */
    memset(text,'x',600); memcpy(text,"note ",5); strcpy(text+600,"\nbackground #000000\nforeground #ffffff\n");
    assert(!parse(&t,text) && r.ignored==1);
    puts("theme file PASS: a whole configuration gives its colours, the rest is counted");
}

static void refusals(void)
{
    static char big[XTHEME_FILE_MAX+2];
    XTheme t;
    assert(refused("{\n  \"name\": \"Dracula\",\n  \"background\": \"#282A36\"\n}\n",XTHEME_NOT_A_THEME_FORMAT));
    message("Dracula.json","built-in dark theme",
            "Settings: theme Dracula.json is not a Ghostty, Kitty or iTerm2 colour file; built-in dark theme");
    assert(refused("# Alacritty\n[colors.primary]\nbackground = '#282a36'\nforeground = '#f8f8f2'\n",XTHEME_NOT_A_THEME_FORMAT));
    assert(refused("bplist00\xd4\x01\x02",XTHEME_BINARY_PLIST));
    message("X.itermcolors","nothing changed",
            "Settings: theme X.itermcolors is a binary property list, export it as XML; nothing changed");
    assert(refused("\xff\xfe" "b",XTHEME_NOT_TEXT));
    assert(refused("\xfe\xff" "b",XTHEME_NOT_TEXT));
    message("u16","built-in dark theme","Settings: theme u16 is not a text file; built-in dark theme");
    theme_default(&t);
    assert(theme_file_parse(&t,"background #000000\0foreground #ffffff\n",38,&r)==-1 && r.error==XTHEME_NOT_TEXT);
    assert(refused("Remember to buy milk.\nAnd bread.\n",XTHEME_NO_COLOURS));
    message("note","built-in dark theme","Settings: theme note has no colours; built-in dark theme");
    assert(refused("",XTHEME_NO_COLOURS));
    assert(refused("background #000000\n",XTHEME_NO_COLOURS));          /* one ground is not a theme */
    assert(refused("font_size 11\ncolor1 #ff0000\n",XTHEME_NO_COLOURS));
    /* A colour key with something that is not a colour: its line. */
    assert(refused("background = #000000\nforeground = #ffffff\npalette = 3=tomato\n",XTHEME_BAD_COLOUR) && r.line==3);
    message("Tomato","built-in dark theme","Settings: theme Tomato line 3: not a colour; built-in dark theme");
    assert(refused("background #00000g\nforeground #ffffff\n",XTHEME_BAD_COLOUR) && r.line==1);
    assert(refused("background #000000\nforeground #ffffff\ncolor256 #ffffff\n",XTHEME_BAD_COLOUR) && r.line==3);
    assert(refused("background #000000\nforeground #ffffff\npalette = 300=#ffffff\n",XTHEME_BAD_COLOUR));
    assert(refused("background #000000\nforeground #fffffff\n",XTHEME_BAD_COLOUR) && r.line==2);
    assert(refused("background\nforeground #ffffff\n",XTHEME_BAD_COLOUR) && r.line==1);
    /* Limpet's own format stays strict: an unknown key in it is an error. */
    assert(refused("ansi1=#ff0000\ncolour=#ffffff\n",XTHEME_BAD_NATIVE));
    message("mine.theme","built-in dark theme","Settings: theme mine.theme is not a valid Limpet theme; built-in dark theme");
    assert(refused("pen3=#ff0000\nforeground=#12345g\n",XTHEME_BAD_NATIVE));
    /* Size. */
    memset(big,'#',sizeof big);
    theme_default(&t);
    assert(theme_file_parse(&t,big,XTHEME_FILE_MAX+1,&r)==-1 && r.error==XTHEME_TOO_LARGE);
    message("big","built-in dark theme","Settings: theme big is over 64 KB; built-in dark theme");
    assert(theme_file_load(&t,"/no-such-limpet-theme",&r)==-1 && r.error==XTHEME_CANNOT_READ);
    message("Gone","built-in dark theme","Settings: theme Gone cannot be read; built-in dark theme");
    puts("theme file PASS: JSON, sections, binary plist, UTF-16, NUL, no colours, bad colours, strict native, size");
}

static void native_as_before(void)
{
    XTheme t,dark;
    /* A native file overrides the built-in dark theme, key by key. */
    theme_default(&dark);
    assert(!parse(&t,"ansi1=#ff0000\n"));
    assert(r.format==XTHEME_FORMAT_NATIVE);
    assert(t.ansi[1]==0xff0000 && t.ansi[2]==dark.ansi[2] && t.background==dark.background);
    assert(!theme_file_load(&t,"themes/dark.theme",&r) && r.format==XTHEME_FORMAT_NATIVE);
    assert(!theme_file_load(&t,"themes/light.theme",&r));
    assert(!theme_file_load(&t,"themes/high-contrast.theme",&r));
    /* The bold switch is a setting: loading a theme keeps it. */
    theme_default(&t); t.bold_bright=0;
    assert(!theme_file_load(&t,"tests/themes/sample.ghostty",&r) && t.bold_bright==0);
    assert(!theme_file_load(&t,"themes/light.theme",&r) && t.bold_bright==0);
    puts("theme file PASS: Limpet's format as before, the bold setting survives a load");
}

/* One colour entry of an iTerm2 preset. space NULL: no Color Space key. */
static size_t entry(char *out,size_t size,const char *key,const char *r_,const char *g,const char *b,const char *space)
{
    char line[96]="";
    if(space) snprintf(line,sizeof line,"\t\t<key>Color Space</key>\n\t\t<string>%s</string>\n",space);
    return (size_t)snprintf(out,size,"\t<key>%s</key>\n\t<dict>\n\t\t<key>Alpha Component</key>\n\t\t<real>1</real>\n"
        "\t\t<key>Blue Component</key>\n\t\t<real>%s</real>\n%s\t\t<key>Green Component</key>\n\t\t<real>%s</real>\n"
        "\t\t<key>Red Component</key>\n\t\t<real>%s</real>\n\t</dict>\n",key,b,line,g,r_);
}
static int near(uint32_t got,uint32_t want)
{
    int i;
    for(i=0;i<24;i+=8) { int d=(int)(got>>i&255)-(int)(want>>i&255); if(d<-1 || d>1) return 0; }
    return 1;
}
static void iterm(void)
{
    static char text[40000];
    static const char head[]="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n";
    XTheme t,native;
    size_t n;
    /* The scheme of the other fixtures, keys out of numeric order. */
    theme_default(&native); theme_default(&t);
    assert(!theme_file_load(&native,"tests/themes/sample.theme",&r));
    assert(!theme_file_load(&t,"tests/themes/sample.itermcolors",&r));
    assert(r.format==XTHEME_FORMAT_ITERM && r.ignored==1);       /* Bold Color */
    assert(!memcmp(&native,&t,sizeof t));
    /* No Color Space (older files), an integer, an exponent; the (Dark)
       and (Light) sets of newer files must not replace the plain keys. */
    n=(size_t)snprintf(text,sizeof text,"%s",head);
    n+=entry(text+n,sizeof text-n,"Background Color (Dark)","1","1","1",NULL);
    n+=entry(text+n,sizeof text-n,"Background Color","0","0","0",NULL);
    n+=entry(text+n,sizeof text-n,"Background Color (Light)","1","1","1","sRGB");
    n+=entry(text+n,sizeof text-n,"Foreground Color","1","5e-1","0.25",NULL);
    n+=entry(text+n,sizeof text-n,"Foreground Color (Light)","0","0","0",NULL);
    n+=entry(text+n,sizeof text-n,"Ansi 1 Color","0.14901961386203766","0.0","1.5",NULL);
    n+=entry(text+n,sizeof text-n,"Ansi 16 Color","1","1","1",NULL);
    n+=entry(text+n,sizeof text-n,"Ansi 2 Color (Dark)","1","1","1",NULL);
    n+=(size_t)snprintf(text+n,sizeof text-n,"</dict>\n</plist>\n");
    theme_default(&t);
    assert(!theme_file_parse(&t,text,n,&r));
    assert(t.background==0x000000 && t.foreground==0xff8040);
    assert(t.ansi[1]==0x2600ff);                /* 0.149 -> 38; 1.5 clamped */
    assert(t.ansi[9]==0x2600ff);                /* bright half derived */
    /* Display P3, against values computed apart from this code. */
    n=(size_t)snprintf(text,sizeof text,"%s",head);
    n+=entry(text+n,sizeof text-n,"Background Color","0","0","0","P3");
    n+=entry(text+n,sizeof text-n,"Foreground Color","1","0","0","P3");
    n+=entry(text+n,sizeof text-n,"Cursor Color","0.2","0.6","0.4","P3");
    n+=entry(text+n,sizeof text-n,"Selection Color","0.9","0.3","0.1","P3");
    n+=entry(text+n,sizeof text-n,"Selected Text Color","0.2","0.6","0.4","sRGB");
    n+=(size_t)snprintf(text+n,sizeof text-n,"</dict>\n</plist>\n");
    theme_default(&t);
    assert(!theme_file_parse(&t,text,n,&r));
    assert(t.foreground==0xff0000 && t.background==0x000000);
    assert(near(t.cursor,0x009c61) && near(t.selection_bg,0xf93b00));
    assert(t.selection_fg==0x339966);
    /* Cut off anywhere: never a crash, and a theme only while enough of it is there. */
    {
        size_t cut;
        FILE *f=fopen("tests/themes/sample.itermcolors","rb");
        assert(f); n=fread(text,1,sizeof text,f); fclose(f);
        for(cut=0;cut<n;cut+=7) {
            char *copy=malloc(cut+1);
            assert(copy); memcpy(copy,text,cut);
            theme_default(&t);
            (void)theme_file_parse(&t,copy,cut,&r);
            free(copy);
        }
        assert(refused("<?xml version=\"1.0\"?>\n<plist version=\"1.0\">\n<dict>\n</dict>\n</plist>\n",XTHEME_NO_COLOURS));
        assert(refused("<plist><dict><key>Background Color</key><dict><key>Red Component</key><real>x</real>"
                       "<key>Green Component</key><real>0</real><key>Blue Component</key><real>0</real></dict></dict></plist>",
                       XTHEME_NO_COLOURS));
    }
    puts("theme file PASS: iTerm2 presets, keys in any order, no colour space, light and dark sets skipped, P3, cut-off files");
}

static const char *present[8];
static int exists(const char *path)
{
    unsigned i;
    for(i=0;i<8 && present[i];++i) if(!strcmp(present[i],path)) return 1;
    return 0;
}
static void locate(void)
{
    char path[80];
    present[0]=NULL;
    assert(!theme_locate(path,sizeof path,"Work:Themes/Nord",exists) && !strcmp(path,"Work:Themes/Nord"));
    assert(!theme_locate(path,sizeof path,"Themes/light.theme",exists) && !strcmp(path,"Themes/light.theme"));
    assert(theme_locate(path,sizeof path,"Dracula",exists)==-1);
    present[0]="PROGDIR:Themes/Dracula.theme"; present[1]=NULL;
    assert(!theme_locate(path,sizeof path,"Dracula",exists) && !strcmp(path,"PROGDIR:Themes/Dracula.theme"));
    /* The user's drawer first, the name as given before any ending. */
    present[1]="ENVARC:Limpet/Themes/Dracula.conf"; present[2]=NULL;
    assert(!theme_locate(path,sizeof path,"Dracula",exists) && !strcmp(path,"ENVARC:Limpet/Themes/Dracula.conf"));
    present[2]="ENVARC:Limpet/Themes/Dracula"; present[3]=NULL;
    assert(!theme_locate(path,sizeof path,"Dracula",exists) && !strcmp(path,"ENVARC:Limpet/Themes/Dracula"));
    present[0]="PROGDIR:Themes/X.itermcolors"; present[1]=NULL;
    assert(!theme_locate(path,sizeof path,"X",exists) && !strcmp(path,"PROGDIR:Themes/X.itermcolors"));
    assert(theme_locate(path,8,"Work:Themes/Nord",exists)==-1);
    assert(theme_locate(path,sizeof path,"",exists)==-1);
    puts("theme file PASS: a name is found in the user's drawer, then the program's, with the usual endings");
}

int main(void)
{
    same_scheme_three_ways();
    variants();
    whole_configuration();
    refusals();
    native_as_before();
    iterm();
    locate();
    return 0;
}
