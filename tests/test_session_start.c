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
    assert(!settings_parse_session(&s, conf, strlen(conf), &w));
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
    assert(build("session_start=fastfetch\nfastfetch_look=file\nfastfetch_config=RAM:mine") == -1 && strstr(why, ".jsonc"));
    assert(build("session_start=fastfetch\nfastfetch_look=file\nfastfetch_config=RAM:Mine.JSON5") == 1);
    {   XSettings s; char line9[600];
        settings_default(&s); s.session_start = XSTART_FASTFETCH;
        memset(s.fastfetch, '*', 250); s.fastfetch[250] = 0;   /* quoted: 502 bytes, the line then too long */
        assert(session_start_line(&s, line9, 510, &why) == -1 && !line9[0] && strstr(why, "too long")); }
    assert(build("session_start=fastfetch\nfastfetch=RAM:$x/ff") == -1 && strstr(why, "program"));
    assert(build("session_start=fastfetch\nfastfetch_look=file\nfastfetch_config=RAM:`x`") == -1 && strstr(why, "configuration"));
    /* The user's own command, as written. */
    assert(build("session_start=command\nsession_command=Echo \"Hello *\"there*\"\"") == 1);
    assert(!strcmp(line, "Echo \"Hello *\"there*\"\""));
    assert(build("session_start=command") == -1 && strstr(why, "no command"));
    {   XSettings s; char small[20];
        settings_default(&s); s.session_start = XSTART_FASTFETCH; s.fastfetch_hide = 15;
        assert(session_start_line(&s, small, sizeof small, &why) == -1 && !small[0] && strstr(why, "too long")); }
    /* As lines of the startup script: Fastfetch only where it is found. */
    {   XSettings s; char script[700], one[700];
        settings_default(&s);
        assert(session_start_script(&s, script, sizeof script, &why) == 0 && !script[0]);
        s.session_start = XSTART_FASTFETCH;
        assert(session_start_line(&s, one, sizeof one, &why) == 1);
        assert(session_start_script(&s, script, sizeof script, &why) == 1);
        assert(!strncmp(script, "Which >NIL: fastfetch\nIf NOT WARN\n", 34));
        assert(strstr(script, one) && strstr(script, "\nElse\nEcho \"Fastfetch is not installed"));
        assert(!strcmp(script + strlen(script) - 6, "\nEndIf"));
        strcpy(s.fastfetch, "Work:My Tools/fastfetch");
        assert(session_start_line(&s, one, sizeof one, &why) == 1);
        assert(session_start_script(&s, script, sizeof script, &why) == 1);
        assert(!strncmp(script, "If EXISTS \"Work:My Tools/fastfetch\"\n", 36));
        assert(strstr(script, one) && strstr(script, "Fastfetch is not at the place set"));
        /* No room for the check: the line alone, as before. */
        assert(session_start_script(&s, script, strlen(one) + 1, &why) == 1 && !strcmp(script, one));
        assert(session_start_script(&s, script, strlen(one), &why) == -1 && !script[0]);
        /* An own command is the user's business: as written. */
        settings_default(&s); s.session_start = XSTART_COMMAND; strcpy(s.session_command, "Echo hello");
        assert(session_start_script(&s, script, sizeof script, &why) == 1 && !strcmp(script, "Echo hello")); }
    puts("session start PASS: off, presets, groups, logo, quoted requester paths, own command, limits, script with a check");
    return 0;
}
