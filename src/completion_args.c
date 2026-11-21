#include "completion_args.h"
#include <string.h>

static unsigned fold(unsigned c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static int same(const char *a, size_t n, const char *b)
{
    size_t i;
    if (strlen(b) != n) return 0;
    for (i = 0; i < n; ++i)
        if (fold((unsigned char)a[i]) != fold((unsigned char)b[i])) return 0;
    return 1;
}
static int blank(char c) { return c == ' ' || c == '\t'; }
static int name_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

static XArg *find(XArgSpec *spec, const char *name, size_t n)
{
    unsigned i;
    for (i = 0; i < spec->count; ++i)
        if (same(name, n, spec->args[i].name) || (spec->args[i].alias[0] && same(name, n, spec->args[i].alias)))
            return &spec->args[i];
    return NULL;
}

/* One template entry: NAME, NAME=ALIAS, then /X modifiers. */
static int entry(XArgSpec *spec, const char *text, size_t n)
{
    XArg *a;
    size_t i = 0, start;
    if (spec->count == XARGS_MAX) return 0;
    a = &spec->args[spec->count];
    memset(a, 0, sizeof *a);
    start = i;
    while (i < n && name_char(text[i])) ++i;
    if (i - start >= XARGS_NAME) return 0;
    memcpy(a->name, text + start, i - start);
    if (i < n && text[i] == '=') {
        start = ++i;
        while (i < n && name_char(text[i])) ++i;
        if (i == start || i - start >= XARGS_NAME) return 0;
        memcpy(a->alias, text + start, i - start);
    }
    while (i < n) {
        if (text[i] != '/' || i + 1 >= n) return 0;
        switch (fold((unsigned char)text[i + 1])) {
        case 'S': a->flags |= XARG_SWITCH; break;
        case 'K': a->flags |= XARG_KEYWORD; break;
        case 'M': a->flags |= XARG_MULTI; break;
        case 'F': a->flags |= XARG_REST; break;
        case 'T': a->flags |= XARG_TOGGLE; break;
        case 'A': case 'N': break;
        default: return 0;
        }
        i += 2;
    }
    /* A nameless entry is a positional argument; a switch needs a name. */
    if (!a->name[0] && (a->flags & (XARG_SWITCH | XARG_KEYWORD | XARG_TOGGLE))) return 0;
    ++spec->count;
    return 1;
}

/* The words of a line after its first, into the pool. */
static int pool(XArgSpec *spec, const char *text, size_t n, unsigned char *at, unsigned char *count)
{
    size_t i = 0;
    *at = (unsigned char)spec->word_count;
    *count = 0;
    while (i < n) {
        size_t start;
        while (i < n && blank(text[i])) ++i;
        start = i;
        while (i < n && !blank(text[i])) ++i;
        if (i == start) break;
        if (i - start >= XARGS_NAME || spec->word_count == XARGS_WORDS) return 0;
        memcpy(spec->words[spec->word_count], text + start, i - start);
        spec->words[spec->word_count++][i - start] = 0;
        ++*count;
    }
    return *count != 0;
}

int completion_args_parse(XArgSpec *spec, const char *text, size_t length)
{
    size_t pos = 0;
    int have_template = 0;
    memset(spec, 0, sizeof *spec);
    if (length > XARGS_DEFINITION_MAX) return 0;
    while (pos < length) {
        size_t end = pos, s, w;
        while (end < length && text[end] != '\n') ++end;
        s = pos;
        w = end;
        pos = end + 1;
        if (w > s && text[w - 1] == '\r') --w;
        while (s < w && blank(text[s])) ++s;
        while (w > s && blank(text[w - 1])) --w;
        if (s == w || text[s] == '#') continue;
        end = s;
        while (end < w && !blank(text[end])) ++end;
#define LINE(word) (end - s == sizeof word - 1 && !memcmp(text + s, word, sizeof word - 1))
        if (LINE("template")) {
            size_t i;
            if (have_template) goto bad;
            have_template = 1;
            while (end < w && blank(text[end])) ++end;
            i = end;
            while (i <= w) {
                size_t e = i;
                while (e < w && text[e] != ',') ++e;
                if (!entry(spec, text + i, e - i)) goto bad;
                i = e + 1;
            }
        } else if (LINE("dirs")) {
            size_t i = end;
            for (;;) {
                size_t e;
                XArg *a;
                while (i < w && blank(text[i])) ++i;
                e = i;
                while (e < w && !blank(text[e])) ++e;
                if (e == i) break;
                if (!(a = find(spec, text + i, e - i)) || (a->flags & XARG_SWITCH)) goto bad;
                a->kind = XARG_DIR;
                i = e;
            }
        } else if (LINE("values")) {
            size_t i = end, e;
            XArg *a;
            while (i < w && blank(text[i])) ++i;
            e = i;
            while (e < w && !blank(text[e])) ++e;
            if (!(a = find(spec, text + i, e - i)) || (a->flags & XARG_SWITCH)) goto bad;
            if (!pool(spec, text + e, w - e, &a->values_at, &a->values_count)) goto bad;
            a->kind = XARG_VALUES;
        } else if (LINE("first")) {
            if (spec->first_count || !pool(spec, text + end, w - end, &spec->first_at, &spec->first_count)) goto bad;
        } else goto bad;
#undef LINE
    }
    if (!have_template && !spec->first_count) goto bad;
    return 1;
bad:
    memset(spec, 0, sizeof *spec);
    return 0;
}

/* The next word of line[*at..end): its bounds without quotes are not
 * needed here, only where it starts and ends and whether '=' follows. */
static int next_word(const char *line, size_t *at, size_t end, size_t *start, size_t *stop)
{
    size_t i = *at;
    int quote = 0;
    while (i < end && (blank(line[i]) || line[i] == '=')) ++i;
    if (i >= end) { *at = i; return 0; }
    *start = i;
    while (i < end) {
        char ch = line[i];
        if (ch == '*' && i + 1 < end) { i += 2; continue; }
        if (ch == '"') quote = !quote;
        else if (!quote && (blank(ch) || ch == '=')) break;
        ++i;
    }
    *stop = i;
    *at = i;
    return 1;
}

int completion_args_command(const char *line, size_t start, char *name, size_t size, size_t *from)
{
    size_t i, segment = 0, at, s, e, base;
    int quote = 0;
    /* The last command of a pipe is the one the word belongs to. */
    for (i = 0; i < start; ++i) {
        char ch = line[i];
        if (ch == '*' && i + 1 < start) { ++i; continue; }
        if (ch == '"') quote = !quote;
        else if (!quote && ch == '|') segment = i + 1;
    }
    if (quote) {
        /* The word itself is inside quotes that opened before it. */
        return 0;
    }
    i = start;
    while (i > segment && blank(line[i - 1])) --i;
    if (i > segment && (line[i - 1] == '<' || line[i - 1] == '>')) return 0;
    at = segment;
    if (!next_word(line, &at, start, &s, &e)) return 0;
    if (line[s] == '"' || line[s] == '<' || line[s] == '>') return 0;
    base = s;
    for (i = s; i < e; ++i)
        if (line[i] == '/' || line[i] == ':') base = i + 1;
    if (e == base || e - base >= size) return 0;
    memcpy(name, line + base, e - base);
    name[e - base] = 0;
    *from = e;
    return 1;
}

static void mark(XArgContext *c, unsigned index) { c->used[index / 32] |= 1u << (index % 32); }
static int marked(const XArgContext *c, unsigned index) { return (c->used[index / 32] >> (index % 32)) & 1; }

/* The next argument a word without a keyword fills. */
static const XArg *positional(const XArgSpec *spec, XArgContext *c, int take)
{
    unsigned i;
    for (i = 0; i < spec->count; ++i) {
        const XArg *a = &spec->args[i];
        if (a->flags & (XARG_SWITCH | XARG_KEYWORD | XARG_TOGGLE)) continue;
        if (marked(c, i)) continue;
        if (take && !(a->flags & (XARG_MULTI | XARG_REST))) mark(c, i);
        return a;
    }
    return NULL;
}

void completion_args_context(const XArgSpec *spec, const char *line, size_t from, size_t start, XArgContext *out)
{
    size_t at = from, s, e;
    const XArg *pending = NULL;
    unsigned words = 0;
    memset(out, 0, sizeof *out);
    while (next_word(line, &at, start, &s, &e)) {
        unsigned i;
        const XArg *named = NULL;
        ++words;
        if (line[s] == '<' || line[s] == '>') { --words; continue; }
        if (pending) { pending = NULL; continue; }       /* a keyword's value */
        if (line[s] != '"')
            for (i = 0; i < spec->count; ++i) {
                const XArg *a = &spec->args[i];
                if (!a->name[0]) continue;
                if (same(line + s, e - s, a->name) || (a->alias[0] && same(line + s, e - s, a->alias))) {
                    named = a;
                    if (!(a->flags & XARG_MULTI)) mark(out, i);
                    break;
                }
            }
        if (named) {
            if (!(named->flags & XARG_SWITCH)) pending = named;
        } else positional(spec, out, 1);
    }
    if (pending) {
        out->value_of = pending;
        out->kind = pending->kind;
        return;
    }
    if (!words && spec->first_count) {
        out->first = 1;
        out->kind = XARG_VALUES;
        return;
    }
    out->names = 1;
    out->value_of = positional(spec, out, 0);
    out->kind = out->value_of ? out->value_of->kind : XARG_ANY;
    /* Allowed words of a positional argument are offered beside the names. */
}

int completion_args_offer(XCompletion *c, const XArgSpec *spec)
{
    char name[XARGS_NAME];
    XArgContext context;
    size_t from;
    unsigned i;
    if (c->command || !completion_args_command(c->expected, c->start, name, sizeof name, &from)) return 0;
    completion_args_context(spec, c->expected, from, c->start, &context);
    if (c->directory[0]) return context.kind == XARG_DIR ? 1 : 0;   /* a path is being typed */
    if (context.first) {
        for (i = 0; i < spec->first_count; ++i) completion_add_word(c, spec->words[spec->first_at + i]);
        return 2;
    }
    if (context.kind == XARG_VALUES && context.value_of)
        for (i = 0; i < context.value_of->values_count; ++i)
            completion_add_word(c, spec->words[context.value_of->values_at + i]);
    if (context.names && c->prefix[0] && !c->quoted)
        for (i = 0; i < spec->count; ++i)
            /* Not the name of the argument this very word would fill:
             * after "CD d" nobody wants "DIR". */
            if (spec->args[i].name[0] && !marked(&context, i) && &spec->args[i] != context.value_of)
                completion_add_word(c, spec->args[i].name);
    if (context.kind == XARG_VALUES) return context.names ? 0 : 2;
    return context.kind == XARG_DIR ? 1 : 0;
}
