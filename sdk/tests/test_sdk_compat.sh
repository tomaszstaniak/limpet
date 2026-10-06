#!/bin/sh
# An SDK client built before the input bit existed must decode the reply the
# terminal makes now exactly as it decoded the reply of the terminal it was
# built with: same status, same fields. The new client reading an old reply
# (input = 0) is in test_sdk.c.
set -eu
# The 0.1.0 sources are vendored (sdk/tests/compat-0.1.0, from ca48999), so
# this runs without git history: shallow CI clones, source archives.
CC=${CC:-cc}
out=build/sdk-compat
old=$out/old
rm -rf "$out"
mkdir -p "$out"
cp -R sdk/tests/compat-0.1.0 "$old"
mkdir -p "$old/sdk/tests"
cp sdk/tests/compat_gen.c "$old/sdk/tests/compat_gen.c"
$CC -std=c99 -Wall -Wextra -Werror sdk/tests/compat_gen.c -o "$out/gen-now"
$CC -std=c99 -Wall -Wextra -Werror "$old/sdk/tests/compat_gen.c" -o "$out/gen-then"
$CC -std=c99 -Wall -Wextra -Werror -I"$old/sdk/include" -Isdk/tests/host \
    sdk/tests/compat_decode.c "$old/sdk/src/limpet_format.c" -o "$out/old-client"
"$out/gen-now" "$out/reply-now.bin"
"$out/gen-then" "$out/reply-then.bin"
now=$("$out/old-client" "$out/reply-now.bin")
then_=$("$out/old-client" "$out/reply-then.bin")
echo "old client, reply then: $then_"
echo "old client, reply now:  $now"
[ "$now" = "$then_" ] || { echo "SDK COMPAT FAIL"; exit 1; }
case "$now" in status=ok*) ;; *) echo "SDK COMPAT FAIL"; exit 1;; esac
echo "SDK COMPAT PASS: an SDK 0.1.0 client reads the current reply as it read its own terminal's"
