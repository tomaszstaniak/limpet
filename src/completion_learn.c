#include "completion_learn.h"
#include "completion_args.h"
#include <string.h>

static int name_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}
static int upper(char c) { return c >= 'A' && c <= 'Z'; }
static char lower(char c) { return upper(c) ? (char)(c + 32) : c; }

typedef struct { char *out; size_t size, used; int full; } Out;
static void put(Out *o, const char *text, size_t n)
{
    if (o->used + n > o->size) { o->full = 1; return; }
    memcpy(o->out + o->used, text, n);
    o->used += n;
}
static void puts_(Out *o, const char *text) { put(o, text, strlen(text)); }

/* Whether hay[0..n) holds the word, whatever its case. */
static int holds(const char *hay, size_t n, const char *word)
{
    size_t w = strlen(word), i, k;
    for (i = 0; i + w <= n; ++i) {
        for (k = 0; k < w && lower(hay[i + k]) == word[k]; ++k) {}
        if (k == w) return 1;
    }
    return 0;
}

/* Writes the words of list[0..n), separated by any of the separators, as
 * the allowed words of an option. Returns how many it wrote. */
static unsigned words(Out *o, const char *list, size_t n, const char *separators)
{
    size_t i = 0;
    unsigned count = 0;
    while (i < n) {
        size_t s;
        while (i < n && (list[i] == ' ' || strchr(separators, list[i]))) ++i;
        s = i;
        while (i < n && list[i] != ' ' && !strchr(separators, list[i])) ++i;
        if (i > s && i - s < XARGS_OPTION_NAME && list[s] != '-' && list[s] != '<') {
            size_t k;
            for (k = s; k < i && name_char(list[k]); ++k) {}
            if (k == i) { puts_(o, " "); put(o, list + s, i - s); ++count; }
        }
    }
    return count;
}

/* One option: the line from its first '-' (line[0..n)) and the lines that
 * continue it (more[0..more_n)). */
static void option(Out *o, const char *line, size_t n, const char *more, size_t more_n)
{
    const char *names[8];
    size_t lengths[8], i = 0, value_s, value_e, k, early_s = 0, early_e = 0;
    unsigned count = 0, pass, written = 0;
    size_t start = o->used;
    int takes = 0;
    const char *possible;

    while (i < n && line[i] == '-') {
        size_t s = i;
        while (i < n && name_char(line[i])) ++i;
        /* -x or --name, and nothing odd inside it ("--colo[u]r" is left out). */
        if (i - s >= 2 && i - s < XARGS_OPTION_NAME && (i == n || strchr(" ,=|[<:\t", line[i])) && count < 8) {
            names[count] = line + s;
            lengths[count++] = i - s;
        } else {
            while (i < n && !strchr(" ,=|\t", line[i])) ++i;
        }
        /* "-o FILE, --output=FILE": the value is named after each spelling. */
        if (i + 1 < n && line[i] == ' ' && line[i + 1] != ' ' && line[i + 1] != '-') {
            for (k = i + 1; k < n && line[k] != ' ' && line[k] != ','; ++k) {}
            if (k + 2 < n && line[k] == ',' && line[k + 1] == ' ' && line[k + 2] == '-') {
                early_s = i + 1;
                early_e = k;
                i = k;
            }
        }
        /* Another spelling follows after "," or "|", or after one space. */
        k = i;
        if (k < n && (line[k] == ',' || line[k] == '|')) ++k;
        while (k < n && line[k] == ' ' && k - i < 2) ++k;
        if (k > i && k < n && line[k] == '-' && !(k - i >= 2 && line[i] == ' ')) i = k;
        else break;
    }
    if (!count) return;

    /* What follows the names: "=X", " <x>", " [<x>]", "[=X]", " X" in
     * capitals. Two spaces or more begin the description. */
    value_s = i;
    if (value_s < n && line[value_s] == '[' && value_s + 1 < n && line[value_s + 1] == '=') { takes = 1; value_s += 2; }
    else if (value_s < n && line[value_s] == '=') { takes = 1; ++value_s; }
    else if (value_s + 1 < n && line[value_s] == ' ' && line[value_s + 1] != ' ') {
        char c = line[value_s + 1], d = value_s + 2 < n ? line[value_s + 2] : 0;
        if (c == '<' || c == '{' || (c == '[' && (d == '<' || d == '?' || upper(d))) || (upper(c) && (upper(d) || d == '_'))) { takes = 1; ++value_s; }
    }
    value_e = value_s;
    if (takes) {
        int depth = 0;
        while (value_e < n) {
            char c = line[value_e];
            if (c == '<' || c == '{' || c == '[') ++depth;
            else if (c == '>' || c == '}' || c == ']') { if (depth) --depth; }
            else if ((c == ' ' || c == '\t' || c == ':') && !depth) break;
            ++value_e;
        }
    }

    if (!takes && early_e > early_s) { takes = 1; value_s = early_s; value_e = early_e; }

    puts_(o, "option");
    for (pass = 0; pass < 2; ++pass)        /* the long spellings first */
        for (k = 0; k < count; ++k)
            if ((lengths[k] > 2 && names[k][1] == '-') == !pass) { puts_(o, " "); put(o, names[k], lengths[k]); }

    if (takes) {
        const char *value = line + value_s;
        size_t vn = value_e - value_s;
        /* clap: "[possible values: always, auto, never]" */
        {
            /* The option's line and the lines that continue it, as one text:
             * the list may be broken between them. */
            static const char key[] = "[possible values:";
            char all[1600];
            size_t an = 0;
            if (n + more_n + 1 <= sizeof all) {
                memcpy(all, line, n);
                an = n;
                all[an++] = ' ';
                memcpy(all + an, more, more_n);
                an += more_n;
            }
            possible = NULL;
            for (k = 0; k + sizeof key - 1 <= an && !possible; ++k)
                if (!memcmp(all + k, key, sizeof key - 1)) {
                    size_t e = k + sizeof key - 1;
                    while (e < an && all[e] != ']') ++e;
                    possible = all + k + sizeof key - 1;
                    written = words(o, possible, (size_t)(all + e - possible), ",\n\r\t");
                }
        }
        if (!written && vn > 2 && (value[0] == '<' || value[0] == '{' || value[0] == '[') &&
            (memchr(value, '|', vn) || (value[0] == '{' && memchr(value, ',', vn))))
            written = words(o, value + 1, vn - 2, "|,<>[]{}");
        if (!written) {
            if (holds(value, vn, "bool")) puts_(o, " true false");
            else if (holds(value, vn, "dir") || holds(value, vn, "folder")) puts_(o, " <dir>");
            else if (holds(value, vn, "file") || holds(value, vn, "path")) puts_(o, " <file>");
            else puts_(o, " <value>");
        }
    }
    puts_(o, "\n");
    if (o->full) { o->used = start; o->full = 1; }     /* never half a line */
}

size_t completion_learn_help(const char *help, size_t length, char *out, size_t size)
{
    /* The text without colour sequences (ESC [ ... letter), in place of the
     * original: lines are short, so a fixed line buffer is enough. */
    char line[512], more[1024];
    size_t pos = 0, n = 0, more_n = 0;
    int have = 0;
    Out o = { out, size, 0, 0 };
    while (pos <= length && !o.full) {
        char cur[512];
        size_t cn = 0, s;
        int end = pos == length;
        while (pos < length && help[pos] != '\n') {
            if (help[pos] == 27 && pos + 1 < length && help[pos + 1] == '[') {
                pos += 2;
                while (pos < length && !((help[pos] >= 'A' && help[pos] <= 'Z') || (help[pos] >= 'a' && help[pos] <= 'z'))) ++pos;
                if (pos < length) ++pos;
                continue;
            }
            if (cn < sizeof cur && help[pos] != '\r') cur[cn++] = help[pos];
            ++pos;
        }
        ++pos;
        for (s = 0; s < cn && (cur[s] == ' ' || cur[s] == '\t'); ++s) {}
        if (end || s == cn || (cur[s] == '-' && s + 1 < cn && (cur[s + 1] == '-' || name_char(cur[s + 1]) || cur[s + 1] == '?'))) {
            /* An option's lines end here: at the next option, an empty
             * line, or the end of the text. */
            if (have) option(&o, line, n, more, more_n);
            have = 0;
            if (!end && s < cn) {
                n = cn - s;
                memcpy(line, cur + s, n);
                more_n = 0;
                have = 1;
            }
        } else if (have && s > 0 && more_n + cn - s + 1 <= sizeof more) {
            memcpy(more + more_n, cur + s, cn - s);
            more_n += cn - s;
            more[more_n++] = ' ';
        } else have = have && s > 0;
        if (end) break;
    }
    return o.used;
}

size_t completion_learn_template(const char *text, size_t length, char *out, size_t size)
{
    size_t s = 0, e;
    XArgSpec spec;
    Out o = { out, size, 0, 0 };
    while (s < length && (text[s] == ' ' || text[s] == '\t' || text[s] == '\r' || text[s] == '\n')) ++s;
    /* The template ends at the ": " of the prompt; what the command says
     * after reading its arguments follows on the same line. */
    e = s;
    while (e < length && text[e] != '\n' && text[e] != '\r' && text[e] != ' ' &&
           !(text[e] == ':' && (e + 1 == length || text[e + 1] == ' ' || text[e + 1] == '\n' || text[e + 1] == '\r'))) ++e;
    if (e == s) return 0;
    /* One bare word is a template only in capitals ("DIR"): "Usage:" and
     * "error:" begin the lines of programs that have none. */
    if (!memchr(text + s, ',', e - s) && !memchr(text + s, '/', e - s)) {
        size_t k;
        for (k = s; k < e; ++k)
            if (text[k] >= 'a' && text[k] <= 'z') return 0;
    }
    puts_(&o, "template ");
    put(&o, text + s, e - s);
    puts_(&o, "\n");
    /* A line that is not a template is not learned. */
    if (o.full || !completion_args_parse(&spec, out, o.used) || !spec.count) return 0;
    return o.used;
}
