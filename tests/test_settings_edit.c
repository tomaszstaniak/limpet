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
    puts("settings edit PASS: other lines kept in place, keys replaced or appended, defaults left out, round trip, size");
    return 0;
}
