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

/* The next word of text[*at..end), blank-separated. */
static int token(const char *text, size_t *at, size_t end, size_t *start, size_t *stop)
{
    size_t i = *at;
    while (i < end && blank(text[i])) ++i;
    *start = i;
    while (i < end && !blank(text[i])) ++i;
    *stop = *at = i;
    return i > *start;
}
static int is(const char *text, size_t n, const char *word) { return strlen(word) == n && !memcmp(text, word, n); }

/* An option line after the word "option": names that start with '-', then
 * nothing, one of <value> <file> <dir>, or the words the value may be.
 * *values is where the part after the names begins. */
static int option_line(const char *text, size_t n, size_t *values)
{
    size_t at = 0, s, e, names = 0, words = 0, marks = 0;
    *values = n;
    while (token(text, &at, n, &s, &e)) {
        size_t i;
        if (text[s] == '-') {
            if (words || marks) return 0;
            if (e - s < 2 || e - s >= XARGS_OPTION_NAME) return 0;
            for (i = s; i < e; ++i)
                if (!name_char(text[i])) return 0;
            ++names;
            continue;
        }
        if (!names) return 0;
        if (!words && !marks) *values = s;
        if (text[s] == '<') {
            if (!is(text + s, e - s, "<value>") && !is(text + s, e - s, "<file>") && !is(text + s, e - s, "<dir>")) return 0;
            ++marks;
        } else {
            if (e - s >= XARGS_OPTION_NAME) return 0;
            ++words;
        }
    }
    return names && marks <= 1 && !(marks && words);
}

/* The option lines of a definition, one after another: the line after the
 * word "option" and where its values begin. */
/* Walks the option lines of a definition. A line "in a b" starts a part
 * whose options belong to the subcommands a and b only; the lines before
 * the first "in" belong to the program as a whole. */
typedef struct {
    size_t pos;
    const char *scope;      /* the words of the current "in" line, or NULL */
    size_t scope_length;
} OptionWalk;

static int in_scope(const OptionWalk *walk, const char *sub, size_t sub_length)
{
    size_t at = 0, s, e;
    if (!walk->scope) return 1;
    if (!sub) return 0;
    while (token(walk->scope, &at, walk->scope_length, &s, &e))
        if (e - s == sub_length && !memcmp(walk->scope + s, sub, sub_length)) return 1;
    return 0;
}

/* The next option line that applies after the subcommand `sub` (NULL: none
 * typed yet): the line after the word "option" and where its values begin. */
static int option_next(const XArgSpec *spec, OptionWalk *walk, const char *sub, size_t sub_length,
                       const char **line, size_t *n, size_t *values)
{
    while (walk->pos < spec->length) {
        size_t end = walk->pos, s, w;
        while (end < spec->length && spec->text[end] != '\n') ++end;
        s = walk->pos;
        w = end;
        walk->pos = end + 1;
        if (w > s && spec->text[w - 1] == '\r') --w;
        while (s < w && blank(spec->text[s])) ++s;
        while (w > s && blank(spec->text[w - 1])) --w;
        if (w - s >= 3 && !memcmp(spec->text + s, "in", 2) && blank(spec->text[s + 2])) {
            walk->scope = spec->text + s + 2;
            walk->scope_length = w - s - 2;
            continue;
        }
        if (w - s < 7 || memcmp(spec->text + s, "option", 6) || !blank(spec->text[s + 6])) continue;
        if (!in_scope(walk, sub, sub_length)) continue;
        *line = spec->text + s + 6;
        *n = w - s - 6;
        if (!option_line(*line, *n, values)) continue;
        return 1;
    }
    return 0;
}

/* The option a word names, exactly as spelled (-l is not -L). */
static int option_find(const XArgSpec *spec, const char *sub, size_t sub_length, const char *word, size_t length,
                       const char **values, size_t *values_length)
{
    OptionWalk walk = { 0, NULL, 0 };
    size_t n, at_values;
    const char *line;
    while (option_next(spec, &walk, sub, sub_length, &line, &n, &at_values)) {
        size_t at = 0, s, e;
        while (at < at_values && token(line, &at, at_values, &s, &e))
            if (e - s == length && !memcmp(line + s, word, length)) {
                *values = line + at_values;
                *values_length = n - at_values;
                return 1;
            }
    }
    return 0;
}

int completion_args_parse(XArgSpec *spec, const char *text, size_t length)
{
    size_t pos = 0;
    int have_template = 0, in_part = 0;
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
        if (in_part && !LINE("option") && !LINE("in")) goto bad;   /* a part holds options only */
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
        } else if (LINE("in")) {
            /* The subcommands the option lines below belong to. */
            size_t at = end, a, b, k, words = 0;
            while (token(text, &at, w, &a, &b)) {
                for (k = a; k < b; ++k)
                    if (!name_char(text[k])) goto bad;
                ++words;
            }
            if (!words) goto bad;
            in_part = 1;
        } else if (LINE("option")) {
            size_t values;
            if (!option_line(text + end, w - end, &values)) goto bad;
            ++spec->options;
        } else goto bad;
#undef LINE
    }
    if (!have_template && !spec->first_count && !spec->options) goto bad;
    spec->text = text;
    spec->length = length;
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
    const char *option = NULL;
    size_t option_length = 0;
    unsigned words = 0;
    memset(out, 0, sizeof *out);
    while (next_word(line, &at, start, &s, &e)) {
        unsigned i;
        const XArg *named = NULL;
        ++words;
        if (line[s] == '<' || line[s] == '>') { --words; continue; }
        if (pending) { pending = NULL; continue; }       /* a keyword's value */
        /* An option's value; a word that starts with '-' is the next
         * option instead, the value was left out. */
        if (option && line[s] != '-') { option = NULL; --words; continue; }
        if (spec->options && line[s] == '-') {
            /* An option is not one of the command's own words. One that
             * takes a value takes the next word. */
            --words;
            if (!option_find(spec, out->sub, out->sub_length, line + s, e - s, &option, &option_length) || !option_length) option = NULL;
            continue;
        }
        if (!out->sub && line[s] != '"') { out->sub = line + s; out->sub_length = e - s; }
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
    if (option) {
        out->option = option;
        out->option_length = option_length;
        out->kind = is(option, option_length, "<dir>") ? XARG_DIR : is(option, option_length, "<file>") ? XARG_ANY : XARG_VALUES;
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
    char name[XARGS_NAME * 2];
    size_t from;
    if (c->command || !completion_args_command(c->expected, c->start, name, sizeof name, &from)) return 0;
    return completion_args_offer_from(c, spec, from);
}

int completion_args_offer_from(XCompletion *c, const XArgSpec *spec, size_t from)
{
    XArgContext context;
    unsigned i;
    if (c->command || from > c->start) return 0;
    completion_args_context(spec, c->expected, from, c->start, &context);
    if (c->directory[0]) return context.kind == XARG_DIR ? 1 : 0;   /* a path is being typed */
    if (spec->options && c->prefix[0] == '-' && !c->quoted) {
        /* The options: their long spellings, or the short one of an
         * option that has no long one. */
        OptionWalk walk = { 0, NULL, 0 };
        size_t n, values;
        const char *line;
        while (option_next(spec, &walk, context.sub, context.sub_length, &line, &n, &values)) {
            size_t at, s, e;
            int pass, added = 0;
            for (pass = 0; pass < 2 && !added; ++pass)
                for (at = 0; at < values && token(line, &at, values, &s, &e);) {
                    char word[XARGS_OPTION_NAME];
                    if (!pass && (e - s < 3 || line[s + 1] != '-')) continue;
                    memcpy(word, line + s, e - s);
                    word[e - s] = 0;
                    completion_add_word(c, word);
                    added = 1;
                }
        }
        return 2;
    }
    if (context.option) {
        size_t at = 0, s, e;
        if (context.kind != XARG_VALUES) return context.kind == XARG_DIR ? 1 : 0;
        while (context.option[0] != '<' && token(context.option, &at, context.option_length, &s, &e)) {
            char word[XARGS_OPTION_NAME];
            memcpy(word, context.option + s, e - s);
            word[e - s] = 0;
            completion_add_word(c, word);
        }
        return 2;
    }
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

int completion_args_program(const char *line, size_t start, char *path, size_t size)
{
    char name[XARGS_NAME * 2];
    size_t from, at, s, e, segment = 0, i;
    int quote = 0;
    if (!completion_args_command(line, start, name, sizeof name, &from)) return 0;
    for (i = 0; i < start; ++i) {
        char ch = line[i];
        if (ch == '*' && i + 1 < start) { ++i; continue; }
        if (ch == '"') quote = !quote;
        else if (!quote && ch == '|') segment = i + 1;
    }
    at = segment;
    if (!next_word(line, &at, start, &s, &e) || e - s >= size) return 0;
    memcpy(path, line + s, e - s);
    path[e - s] = 0;
    return 1;
}

int completion_args_run(const char *line, size_t from, size_t start, const char *name,
                        char *id, size_t size, size_t *id_from, char *root, size_t root_size)
{
    size_t at = from, s, e, i;
    root[0] = 0;
    if (!same(name, strlen(name), "apkg")) return 0;
    if (!next_word(line, &at, start, &s, &e)) return 0;
    if (e - s == 6 && !memcmp(line + s, "--root", 6)) {
        /* apkg --root DRAWER run ...: the package is under that drawer. */
        if (!next_word(line, &at, start, &s, &e) || line[s] == '"' || line[s] == '-' || e - s >= root_size) return 0;
        memcpy(root, line + s, e - s);
        root[e - s] = 0;
        if (!next_word(line, &at, start, &s, &e)) return 0;
    }
    if (!same(line + s, e - s, "run")) return 0;
    if (!next_word(line, &at, start, &s, &e) || line[s] == '-' || e - s >= size) return 0;
    for (i = s; i < e; ++i)
        if (!name_char(line[i])) return 0;
    memcpy(id, line + s, e - s);
    id[e - s] = 0;
    *id_from = e;
    return 1;
}
