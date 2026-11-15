#include "../examples/ssh/scp_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int device(const char *name, size_t length)
{
    return (length == 3 && !memcmp(name, "RAM", 3)) || (length == 4 && !memcmp(name, "Work", 4));
}

static void split(const char *argument, int remote, const char *user, const char *host, const char *path)
{
    struct xt_scp_spec s;
    assert(xt_scp_split(argument, device, &s) == 0);
    assert(s.remote == remote);
    assert(!strcmp(s.user, user));
    assert(!strcmp(s.host, host));
    assert(!strcmp(s.path, path));
}

static void name(const char *path, const char *expected)
{
    size_t length;
    const char *start = xt_scp_remote_name(path, &length);
    assert(length == strlen(expected) && !memcmp(start, expected, length));
}

int main(void)
{
    struct xt_scp_spec s;
    char out[16];
    char longhost[400];

    split("notes.txt", 0, "", "", "notes.txt");
    split("RAM:notes.txt", 0, "", "", "RAM:notes.txt");
    split("Work:dir/a:b", 0, "", "", "Work:dir/a:b");
    split(":notes.txt", 0, "", "", ":notes.txt");
    split("dir/a:b", 0, "", "", "dir/a:b");
    split("server:notes.txt", 1, "", "server", "notes.txt");
    split("me@server:", 1, "me", "server", ".");
    split("me@server:~", 1, "me", "server", ".");
    split("me@server:~/x", 1, "me", "server", "x");
    split("me@server:/etc/motd", 1, "me", "server", "/etc/motd");
    split("me@RAM:x", 1, "me", "RAM", "x");                 /* '@' decides */
    split("me@corp@server:x", 1, "me@corp", "server", "x");
    split("192.168.1.2:a b", 1, "", "192.168.1.2", "a b");
    assert(xt_scp_split("me@:x", device, &s) == -1);
    memset(longhost, 'h', sizeof longhost - 3);
    strcpy(longhost + sizeof longhost - 3, ":x");
    assert(xt_scp_split(longhost, device, &s) == -1);

    name("a/b/c.txt", "c.txt");
    name("dir/", "dir");
    name("/", "");
    name(".", "");
    name("a/..", "");
    name("x", "x");

    assert(xt_scp_remote_join(out, sizeof out, "a", "b") == 0 && !strcmp(out, "a/b"));
    assert(xt_scp_remote_join(out, sizeof out, "/", "b") == 0 && !strcmp(out, "/b"));
    assert(xt_scp_remote_join(out, sizeof out, "a/", "b") == 0 && !strcmp(out, "a/b"));
    assert(xt_scp_remote_join(out, sizeof out, "0123456789", "abcdef") == -1);

    assert(xt_scp_local_name(out, sizeof out, "a:b", 3) == 0 && !strcmp(out, "a_b"));
    assert(xt_scp_local_name(out, sizeof out, "..", 2) == -1);
    assert(xt_scp_local_name(out, sizeof out, ".", 1) == -1);
    assert(xt_scp_local_name(out, sizeof out, "a/b", 3) == -1);
    assert(xt_scp_local_name(out, sizeof out, "", 0) == -1);
    assert(xt_scp_local_name(out, sizeof out, ".profile", 8) == 0);
    {
        /* a:b.txt and a_b.txt get the same AROS name; Foo and foo are one
         * file there. The second of each pair must be refused. */
        struct xt_scp_names names = { 0, 0, 0 };
        char first[16], second[16];
        int i;
        assert(xt_scp_local_name(first, sizeof first, "a:b.txt", 7) == 0);
        assert(xt_scp_local_name(second, sizeof second, "a_b.txt", 7) == 0);
        assert(xt_scp_names_claim(&names, first, 1) == 0);
        assert(xt_scp_names_claim(&names, second, 1) == 1);
        assert(xt_scp_names_claim(&names, "Foo", 1) == 0);
        assert(xt_scp_names_claim(&names, "foo", 1) == 1);
        assert(xt_scp_names_claim(&names, "FOO", 1) == 1);
        assert(xt_scp_names_claim(&names, "foo2", 1) == 0);
        assert(xt_scp_names_claim(&names, "fo", 1) == 0);
        assert(xt_scp_names_claim(&names, "\xc9t\xe9", 1) == 0);      /* Latin-1 capital and small e acute */
        assert(xt_scp_names_claim(&names, "\xe9t\xe9", 1) == 1);
        for (i = 0; i < 100; ++i)
        {
            char name[16];
            snprintf(name, sizeof name, "file%d", i);
            assert(xt_scp_names_claim(&names, name, 1) == 0);
        }
        assert(xt_scp_names_claim(&names, "FILE57", 1) == 1);
        xt_scp_names_free(&names);
        /* On a server the case of letters tells names apart. */
        assert(xt_scp_names_claim(&names, "Foo", 0) == 0);
        assert(xt_scp_names_claim(&names, "foo", 0) == 0);
        assert(xt_scp_names_claim(&names, "foo", 0) == 1);
        xt_scp_names_free(&names);
    }
    puts("ssh scp spec: PASS");
    return 0;
}
