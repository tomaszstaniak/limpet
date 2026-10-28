#include "session_start.h"
#include "session_command.h"
#include <string.h>

static int append(char *out, size_t capacity, size_t *n, const char *text)
{
    size_t k = strlen(text);
    if (*n + k >= capacity) return 0;
    memcpy(out + *n, text, k + 1);
    *n += k;
    return 1;
}

int session_start_line(const XSettings *s, char *out, size_t capacity, const char **why)
{
    char quoted[2 * XSETTINGS_PATH_MAX + 3];
    size_t n = 0;
    unsigned i;
    *why = NULL;
    if (!capacity) { *why = "no room for the command"; return -1; }
    out[0] = 0;
    if (s->session_start == XSTART_OFF) return 0;
    if (s->session_start == XSTART_COMMAND) {
        if (!s->session_command[0]) { *why = "no command is set"; return -1; }
        if (strpbrk(s->session_command, "\r\n")) { *why = "the command has a line end"; return -1; }
        if (!append(out, capacity, &n, s->session_command)) { *why = "the command is too long"; return -1; }
        return 1;
    }
    /* A path chosen in a requester may hold spaces and quotes: quoted and
       escaped for the Shell; substitutions ($, `, <) are refused. */
    if (s->fastfetch[0]) {
        if (session_quote_path(s->fastfetch, quoted, sizeof quoted)) {
            *why = "the Fastfetch program's name cannot be used in a Shell line"; return -1;
        }
        append(out, capacity, &n, quoted);
    } else append(out, capacity, &n, "fastfetch");
    if (s->fastfetch_look == XFF_FILE) {
        if (!s->fastfetch_config[0]) { *why = "no configuration file is chosen"; return -1; }
        if (session_quote_path(s->fastfetch_config, quoted, sizeof quoted)) {
            *why = "the configuration file's name cannot be used in a Shell line"; return -1;
        }
        if (!append(out, capacity, &n, " -c ") || !append(out, capacity, &n, quoted)) goto long_line;
    } else if (!append(out, capacity, &n, s->fastfetch_look == XFF_PICTURE ? " -c aros-logo" : " -c aros-compact"))
        goto long_line;
    if (s->fastfetch_hide) {
        const char *separator = " --structure-disabled ";
        for (i = 0; i < XFF_GROUPS; ++i)
            if (s->fastfetch_hide & (1u << i)) {
                if (!append(out, capacity, &n, separator) || !append(out, capacity, &n, settings_group_names[i]))
                    goto long_line;
                separator = ":";
            }
    }
    if (s->fastfetch_logo == XFF_LOGO_TEXT && !append(out, capacity, &n, " --logo-type small --logo aros"))
        goto long_line;
    if (s->fastfetch_logo == XFF_LOGO_NONE && !append(out, capacity, &n, " --logo none")) goto long_line;
    return 1;
long_line:
    *why = "the command is too long";
    out[0] = 0;
    return -1;
}
