#include "../src/session_start.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char line[512];
static const char *why;
static int build(const char *conf)
{
    XSettings s; const char *w;
    settings_default(&s);
    assert(!settings_parse(&s, conf, strlen(conf), &w));
    return session_start_line(&s, line, sizeof line, &why);
}
int main(void)
{
    assert(build("") == 0 && !line[0]);
    assert(build("session_start=off\nsession_command=Echo hi\n") == 0);
    /* Fastfetch: a preset by name, never a copy of it. */
    assert(build("session_start=fastfetch") == 1 && !strcmp(line, "fastfetch -c aros-compact"));
    assert(build("session_start=fastfetch\nfastfetch_look=picture") == 1 && !strcmp(line, "fastfetch -c aros-logo"));
    assert(build("session_start=fastfetch\nfastfetch_look=picture\nfastfetch_hide=network,system\nfastfetch_logo=text") == 1);
    assert(!strcmp(line, "fastfetch -c aros-logo --structure-disabled system:network --logo-type small --logo aros"));
    assert(build("session_start=fastfetch\nfastfetch_logo=none\nfastfetch_hide=memory-disks") == 1);
    assert(!strcmp(line, "fastfetch -c aros-compact --structure-disabled memory-disks --logo none"));
    /* Requester paths: quoted, quotes and stars escaped. */
    assert(build("session_start=fastfetch\nfastfetch=Work:My Tools/fastfetch\nfastfetch_look=file\n"
                 "fastfetch_config=ENVARC:a \"b\" *.jsonc") == 1);
    assert(!strcmp(line, "\"Work:My Tools/fastfetch\" -c \"ENVARC:a *\"b*\" **.jsonc\""));
    assert(build("session_start=fastfetch\nfastfetch_look=file") == -1 && strstr(why, "no configuration file"));
    assert(build("session_start=fastfetch\nfastfetch=RAM:$x/ff") == -1 && strstr(why, "program"));
    assert(build("session_start=fastfetch\nfastfetch_look=file\nfastfetch_config=RAM:`x`") == -1 && strstr(why, "configuration"));
    /* The user's own command, as written. */
    assert(build("session_start=command\nsession_command=Echo \"Hello *\"there*\"\"") == 1);
    assert(!strcmp(line, "Echo \"Hello *\"there*\"\""));
    assert(build("session_start=command") == -1 && strstr(why, "no command"));
    {   XSettings s; char small[20];
        settings_default(&s); s.session_start = XSTART_FASTFETCH; s.fastfetch_hide = 15;
        assert(session_start_line(&s, small, sizeof small, &why) == -1 && !small[0] && strstr(why, "too long")); }
    puts("session start PASS: off, presets, groups, logo, quoted requester paths, own command, limits");
    return 0;
}
