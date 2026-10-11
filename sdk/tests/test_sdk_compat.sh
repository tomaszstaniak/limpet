#!/bin/sh
# An SDK client built before a field existed must decode the reply the
# terminal makes now exactly as it decoded the reply of the terminal it was
# built with: same status, same fields. Checked for every published or
# vendored SDK: 0.1.0 (before the input bit) and 0.2.0 (before the feature
# bit). The new client reading an old reply is in test_sdk.c.
set -eu
# The old sources are vendored (sdk/tests/compat-<version>), so this runs
# without git history: shallow CI clones, source archives.
CC=${CC:-cc}
out=build/sdk-compat
rm -rf "$out"
mkdir -p "$out"
$CC -std=c99 -Wall -Wextra -Werror sdk/tests/compat_gen.c -o "$out/gen-now"
"$out/gen-now" "$out/reply-now.bin"
for version in 0.1.0 0.2.0; do
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
    [ "$now" = "$then_" ] || { echo "SDK COMPAT FAIL ($version)"; exit 1; }
    case "$now" in status=ok*) ;; *) echo "SDK COMPAT FAIL ($version)"; exit 1;; esac
    echo "SDK COMPAT PASS: an SDK $version client reads the current reply as it read its own terminal's"
    # Binary guard: an object compiled with the old header must not link
    # against the current library (its LimpetPresentation is shorter).
    $CC -std=c99 -Wall -Wextra -Werror -I"$old/sdk/include" -Isdk/tests/host -c sdk/tests/compat_decode.c -o "$old/client.o"
    if $CC "$old/client.o" sdk/src/limpet_format.c -Isdk/include -Isdk/tests/host -o "$old/mixed" 2>/dev/null; then
        echo "SDK BINARY GUARD FAIL: an object built with the $version header linked against the new library"; exit 1
    fi
    echo "SDK BINARY GUARD PASS: an object built with the $version header must be rebuilt"
done
