#!/bin/sh
# An SDK client built before the input bit existed must decode the reply the
# terminal makes now exactly as it decoded the reply of the terminal it was
# built with: same status, same fields. The new client reading an old reply
# (input = 0) is in test_sdk.c.
set -eu
BASE=${SDK_COMPAT_BASE:-ca48999}     # the SDK 0.1.0 working state
CC=${CC:-cc}
out=build/sdk-compat
old=$out/old
rm -rf "$out"
mkdir -p "$old/sdk/include/limpet" "$old/sdk/src" "$old/sdk/tests" "$old/src"
for f in sdk/include/limpet/limpet.h sdk/include/limpet/tty_control.h sdk/src/limpet_format.c \
         src/xterm_presentation.h src/tty_control.h; do
    git show "$BASE:$f" > "$old/$f"
done
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
echo "SDK COMPAT PASS: an SDK from $BASE reads the current reply as it read its own terminal's"
