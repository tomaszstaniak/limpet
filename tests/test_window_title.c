/* What the window title says, in which order, in which character set, and
 * how long a program's own title lasts. */
#include "../src/window_title.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *compose(const XWindowTitle *in)
{
    static char out[160];
    window_title_compose(out, sizeof out, in);
    return out;
}

static XForeground fg(int known, int running, unsigned long module, const char *name)
{
    XForeground f;
    memset(&f, 0, sizeof f);
    f.known = known; f.running = running; f.module = module;
    snprintf(f.name, sizeof f.name, "%s", name ? name : "");
    return f;
}

int main(void)
{
    XWindowTitle in;
    XProgramTitle t;
    XForeground idle = fg(1, 0, 0, "Shell Process"), hc = fg(1, 1, 0x1000, "Work:hc/hardcode"),
                wait = fg(1, 1, 0x2000, "Wait"), unknown = fg(0, 0, 0, NULL);
    char small[16];

    memset(&in, 0, sizeof in);
    in.terminal = "Limpet";
    assert(!strcmp(compose(&in), "Limpet"));
    in.shell = 1;
    assert(!strcmp(compose(&in), "Shell - Limpet"));
    in.command = "Work:hc/hardcode";
    assert(!strcmp(compose(&in), "hardcode - Limpet"));
    in.program = "Hardcode: waiting for the model";
    assert(!strcmp(compose(&in), "Hardcode: waiting for the model - Limpet"));
    /* The status follows the name; the name is never pushed out by it. */
    in.status = "Copied";
    assert(!strcmp(compose(&in), "Hardcode: waiting for the model - Limpet | Copied"));
    in.view_offset = 3; in.history_count = 200;
    assert(!strcmp(compose(&in), "Hardcode: waiting for the model - Limpet | Copied | scrollback 3/200"));
    /* A link under the pointer comes first, for as long as it is there. */
    in.link = "https://aros.org/";
    assert(!strcmp(compose(&in), "Link: https://aros.org/ - Limpet | Copied | scrollback 3/200"));
    in.link = NULL; in.status = NULL; in.view_offset = 0;
    /* An empty program title is no title. */
    in.program = "";
    assert(!strcmp(compose(&in), "hardcode - Limpet"));

    /* Program titles are UTF-8; the title bar is Latin-1. */
    in.program = "Gr\xc3\xbc\xc3\x9f" "e \xc5\xbc\xc3\xb3\xc5\x82w \xe6\xbc\xa2 \xf0\x9f\x98\x80";
    assert(!strcmp(compose(&in), "Gr\xfc\xdf" "e ?\xf3?w ? ? - Limpet"));
    in.program = "bad \xc3 \xff end";
    assert(!strcmp(compose(&in), "bad ? ? end - Limpet"));
    /* Command names are system-charset bytes: kept, except controls. */
    in.program = NULL; in.command = "RAM:caf\xe9\x1b[2J";
    assert(!strcmp(compose(&in), "caf\xe9?[2J - Limpet"));
    in.command = "RAM:dir/";      /* nothing after the last separator */
    assert(!strcmp(compose(&in), "Shell - Limpet"));
    assert(!strcmp(window_title_command_name("C:Wait"), "Wait"));
    assert(!strcmp(window_title_command_name("Wait"), "Wait"));
    assert(!strcmp(window_title_command_name("A:b/c/d"), "d"));

    /* Cut to the buffer, always terminated. */
    in.command = "Wait"; in.status = "a long status message";
    assert(window_title_compose(small, sizeof small, &in) == 15 && !strcmp(small, "Wait - Limpet |"));
    assert(window_title_compose(small, 1, &in) == 0 && !small[0]);
    {   /* A long URL: its start, then "...". */
        char url[300]; char out[200];
        memset(url, 'u', sizeof url - 1); url[sizeof url - 1] = 0;
        memset(&in, 0, sizeof in); in.terminal = "Limpet"; in.link = url;
        window_title_compose(out, sizeof out, &in);
        assert(strlen(out) == 6 + 100 + 3 + 9 && !strcmp(out + 109, " - Limpet"));
    }

    /* Lifetime: the title belongs to the command that sent it. */
    memset(&t, 0, sizeof t);
    program_title_offer(&t, "Hardcode: ready", 1, &hc);
    assert(!strcmp(program_title_text(&t), "Hardcode: ready"));
    assert(!program_title_follow(&t, &hc));
    assert(!program_title_follow(&t, &unknown));   /* nothing readable: kept */
    assert(program_title_follow(&t, &idle) && !program_title_text(&t));
    /* The same serial again is not a new title. */
    program_title_offer(&t, "Hardcode: ready", 1, &hc);
    assert(!program_title_text(&t));
    program_title_offer(&t, "Hardcode: replying", 2, &hc);
    assert(program_title_follow(&t, &wait) && !program_title_text(&t));
    /* An empty title drops it at once. */
    program_title_offer(&t, "x", 3, &hc);
    program_title_offer(&t, "", 4, &hc);
    assert(!program_title_text(&t));
    /* Arrived while nothing could be read: kept until cleared. */
    program_title_offer(&t, "y", 5, &unknown);
    assert(!program_title_follow(&t, &idle) && !program_title_follow(&t, &wait));
    program_title_clear(&t);
    assert(!program_title_text(&t) && t.serial == 5);
    /* Sent from the prompt (a background job): ends when a command starts. */
    program_title_offer(&t, "z", 6, &idle);
    assert(!program_title_follow(&t, &idle) && program_title_follow(&t, &wait));
    puts("WINDOW TITLE PASS: order link > program > command > Shell, status after the name, "
         "Latin-1 mapping, control filtering, truncation, program title lifetime");
    return 0;
}
