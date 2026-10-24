#!/bin/sh
# An SDK client built before a field existed must decode the reply the
# terminal makes now exactly as it decoded the reply of the terminal it was
# built with: same status, same fields. Checked for every published or
# vendored SDK: 0.1.0 (before the input bit), 0.2.0 (before the feature
# bit), 0.3.0 (before the Shift+Enter input bit) and 0.4.0 (before the
# OSC 8, icon and further style bits). The new client reading
# an old reply is in test_sdk.c.
set -eu
# The old sources are vendored (sdk/tests/compat-<version>), so this runs
# without git history: shallow CI clones, source archives.
CC=${CC:-cc}
out=build/sdk-compat
rm -rf "$out"
mkdir -p "$out"
$CC -std=c99 -Wall -Wextra -Werror sdk/tests/compat_gen.c -o "$out/gen-now"
"$out/gen-now" "$out/reply-now.bin"
# Each version with the input bits its header defines, and whether an object
# built with its header may link against the current library (0.4.0 only
# added a bit, so 0.3.0 objects still fit, as 0.5.0 only adds bits for 0.4.0;
# 0.1 and 0.2 structs are shorter).
for entry in 0.1.0:0:no 0.2.0:7:no 0.3.0:7:yes 0.4.0:15:yes; do
    version=${entry%%:*}; rest=${entry#*:}; known=${rest%%:*}; links=${rest#*:}
    old=$out/old-$version
    cp -R sdk/tests/compat-$version "$old"
    mkdir -p "$old/sdk/tests"
    cp sdk/tests/compat_gen.c "$old/sdk/tests/compat_gen.c"
    $CC -std=c99 -Wall -Wextra -Werror "$old/sdk/tests/compat_gen.c" -o "$old/gen-then"
    $CC -std=c99 -Wall -Wextra -Werror -I"$old/sdk/include" -Isdk/tests/host \
        sdk/tests/compat_decode.c "$old/sdk/src/limpet_format.c" -o "$old/client"
    "$old/gen-then" "$old/reply-then.bin"
    now=$("$old/client" "$out/reply-now.bin")
    then_=$("$old/client" "$old/reply-then.bin")
    echo "SDK $version client, reply then: $then_"
    echo "SDK $version client, reply now:  $now"
    # Input bits added after this SDK are new capabilities in the same field:
    # a client ignores bits it does not know, so they are taken out before
    # comparing. Every bit the old client knew must be the same.
    case "$now" in *input=*)
        bits=${now##*input=}; bits=${bits%% *}
        now="${now%%input=*}input=$((bits & known))${now#*input=$bits}";;
    esac
    [ "$now" = "$then_" ] || { echo "SDK COMPAT FAIL ($version)"; exit 1; }
    case "$now" in status=ok*) ;; *) echo "SDK COMPAT FAIL ($version)"; exit 1;; esac
    echo "SDK COMPAT PASS: an SDK $version client reads the current reply as it read its own terminal's"
    # Binary guard: an object compiled with a header whose LimpetPresentation
    # is shorter must not link against the current library; one with the same
    # layout must link and decode the current reply as the current SDK does.
    $CC -std=c99 -Wall -Wextra -Werror -I"$old/sdk/include" -Isdk/tests/host -c sdk/tests/compat_decode.c -o "$old/client.o"
    if [ "$links" = no ]; then
        if $CC "$old/client.o" sdk/src/limpet_format.c -Isdk/include -Isdk/tests/host -o "$old/mixed" 2>/dev/null; then
            echo "SDK BINARY GUARD FAIL: an object built with the $version header linked against the new library"; exit 1
        fi
        echo "SDK BINARY GUARD PASS: an object built with the $version header must be rebuilt"
    else
        $CC "$old/client.o" sdk/src/limpet_format.c -Isdk/include -Isdk/tests/host -o "$old/mixed"
        mixed=$("$old/mixed" "$out/reply-now.bin")
        [ "$mixed" = "$("$old/client" "$out/reply-now.bin")" ] || { echo "SDK BINARY FAIL ($version): $mixed"; exit 1; }
        echo "SDK BINARY PASS: an object built with the $version header links against the new library and reads the reply"
    fi
done
