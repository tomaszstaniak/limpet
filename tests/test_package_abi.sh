#!/bin/zsh
# make-packages.sh must refuse binaries of another target or ABI. Needs the
# three targets built (make target TARGET=...); works on copies of them.
#   tests/test_package_abi.sh VERSION
set -euo pipefail
ver=$1 root=${0:A:h:h}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/limpet-package-abi.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
v11=aros-x86_64-abiv11-pc v1=aros-x86_64-abiv1-pc pi=aros-aarch64-abiv1-raspi
fresh() { rm -rf "$tmp/build"; mkdir -p "$tmp/build"; for t in $v11 $v1 $pi; do cp -R "$root/build/$t" "$tmp/build/$t"; done; }
pack() { rm -rf "$tmp/out"; LIMPET_PACKAGE_BUILD="$tmp/build" "$root/tools/release/make-packages.sh" "$ver" "$tmp/out" "$@" >"$tmp/log" 2>&1; }
expect_refused() {  # $1 = case, $2 = message part, rest = targets
    local name=$1 want=$2; shift 2
    if pack "$@"; then print -u2 "FAIL $name: packaged"; exit 1; fi
    grep -q -- "$want" "$tmp/log" || { print -u2 "FAIL $name: other refusal:"; cat "$tmp/log" >&2; exit 1; }
    print "pass $name"
}
fresh; pack $v11 $v1 $pi || { cat "$tmp/log" >&2; print -u2 "FAIL clean builds refused"; exit 1; }; print "pass clean"
fresh; cp "$tmp/build/$v1/limpet-handler-remove" "$tmp/build/$v11/"
expect_refused "mainline remover in ABIv11" "limpet-handler-remove has no __aros_libset_StdlibBase" $v11
fresh; cp "$tmp/build/$v11/limpet-handler-remove" "$tmp/build/$v1/"
expect_refused "ABIv11 remover in mainline" "limpet-handler-remove has no __aros_libset_StdCBase" $v1
fresh; cp "$tmp/build/$v1/limpet-handler" "$tmp/build/$v11/"
expect_refused "mainline handler in ABIv11" "differs from what make target built" $v11
fresh; cp "$tmp/build/$v11/Limpet" "$tmp/build/$v1/"
expect_refused "ABIv11 program in mainline" "Limpet has no __aros_libset_StdCBase" $v1
fresh; rm -rf "$tmp/build/$v11"; cp -R "$tmp/build/$v1" "$tmp/build/$v11"
expect_refused "mainline build directory as ABIv11" "has no __aros_libset_CrtBase" $v11
fresh; cp "$tmp/build/$pi/limpet-handler" "$tmp/build/$v1/"
expect_refused "Pi handler in mainline x86_64" "is not an ELF for" $v1
print "all package ABI checks pass"
