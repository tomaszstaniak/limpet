#!/bin/sh
# Build the SSH programs (ssh, scp, ssh-keygen, ssh-copy-id) for one target:
#
#     AROS_GCC=... AROS_SDK=... AROS_CRYPTO=... sh tools/build-ssh.sh TARGET
#
# TARGET is aros-x86_64-abiv1-pc, aros-aarch64-abiv1-raspi or
# aros-x86_64-abiv11-pc; AROS_GCC is that target's C compiler driver,
# AROS_SDK its Developer SDK and AROS_CRYPTO the aros-crypto kit built for the
# same target (https://github.com/tomaszstaniak/aros-crypto: include/, lib/).
# LIMPET_VERSION and LIMPET_DATE (dd.mm.yyyy) name the build.
# Output: build/TARGET/ssh/: the four programs, licences/ and build-record.
set -eu
cd "$(dirname "$0")/.."
target=${1:?target name}
AROS_GCC=${AROS_GCC:?set AROS_GCC to the target compiler}
AROS_SDK=${AROS_SDK:?set AROS_SDK to the target SDK}
AROS_CRYPTO=${AROS_CRYPTO:?set AROS_CRYPTO to the aros-crypto kit of this target}
AROS_CRYPTO=$(cd "$AROS_CRYPTO" && pwd)
grep -q "for $target\$" "$AROS_CRYPTO/BUILD.txt" ||
    { echo "build-ssh: $AROS_CRYPTO is not an aros-crypto kit for $target" >&2; exit 1; }
case "$target" in
aros-x86_64-abiv1-pc)
    processor=x86_64; relocations='R_X86_64_[^[:space:]]*(GOT|TLS|TPOFF)'
    libs='-lnet -lposixc -lstdcio -lstdc -lautoinit' ;;
aros-aarch64-abiv1-raspi)
    processor=aarch64; relocations='R_AARCH64_[^[:space:]]*(GOT|TLS)'
    libs='-lnet -lposixc -lstdcio -lstdc -lautoinit' ;;
aros-x86_64-abiv11-pc)
    processor=x86_64; relocations='R_X86_64_[^[:space:]]*(GOT|TLS|TPOFF)'
    libs='-lnet'; define=-DXT_SSH_ABIV11 ;;
*) echo "build-ssh: unknown target $target" >&2; exit 1 ;;
esac
define=${define:-}
tool=${AROS_GCC%-*}
root=$(pwd)
out=build/$target/ssh
mkdir -p build/ssh "$out"
# fetch NAME URL SHA256: a pinned source archive, checked on every build.
fetch() {
    if [ ! -f "build/ssh/$1" ]; then
        curl -fL --proto '=https' "$2" -o "build/ssh/$1.part"
        mv "build/ssh/$1.part" "build/ssh/$1"
    fi
    test "$(shasum -a 256 "build/ssh/$1" | cut -d ' ' -f 1)" = "$3"
}
fetch libssh-0.12.2.tar.xz https://www.libssh.org/files/0.12/libssh-0.12.2.tar.xz \
    49560f677d96e3706a904ac2de1116e25f3680937d51e5c92198fcba4a1c1e9f
source=build/ssh/libssh-0.12.2
# Apply the patch once, to a fresh tree: one patched by an older version of
# the patch is unpacked again. A reverse dry run does not tell whether the
# patch is in: some patch programs report success for it on an unpatched tree.
if ! grep -q xt_ssh_break_wanted "$source/src/poll.c" 2>/dev/null; then
    rm -rf build/ssh/libssh-0.12.2
    tar -xf build/ssh/libssh-0.12.2.tar.xz -C build/ssh
    patch -d "$source" -p1 < tools/ssh/libssh-aros.patch
fi
grep -q xt_ssh_break_wanted "$source/src/poll.c"
grep -q xt_ssh_home_dir "$source/src/misc.c"
grep -q "no key may be made" "$source/src/libmbedcrypto.c"
grep -q "aros_crypto_setup" "$source/src/threads/mbedtls.c"
grep -q XT_SSH_BSDSOCKET_DIRECT "$source/src/socket.c"
grep -q "NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE" "$source/cmake/Modules/DefineCMakeDefaults.cmake"
# libssh must not initialise itself before main: the client sets the kit up
# first (aros_crypto_setup) and then calls ssh_init(). No compiler
# thread-local storage: the AROS loader has no TLS relocations.
cmake -S "$source" -B "$out/libssh" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=OFF \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=$processor \
    -DCMAKE_C_COMPILER="$AROS_GCC" \
    -DCMAKE_AR="$tool-ar" -DCMAKE_RANLIB="$tool-ranlib" \
    -DCMAKE_SYSROOT="$AROS_SDK" -DCMAKE_FIND_ROOT_PATH="$AROS_SDK" \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_C_FLAGS="-include \"$root/tools/ssh/aros-platform.h\" -Werror=implicit-function-declaration -Werror=int-conversion -ffile-prefix-map=$root=. $define" \
    -DCMAKE_EXE_LINKER_FLAGS='-Wl,-u,SocketBase -lnet' \
    -DBUILD_SHARED_LIBS=OFF -DWITH_EXAMPLES=OFF -DWITH_GSSAPI=OFF \
    -DWITH_ZLIB=OFF -DWITH_SERVER=OFF -DWITH_SFTP=ON -DWITH_NACL=OFF \
    -DWITH_EXEC=OFF -DWITH_PCAP=OFF -DCMAKE_BUILD_TYPE=Release \
    -DWITH_MBEDTLS=ON -DMBEDTLS_INCLUDE_DIR="$AROS_CRYPTO/include" \
    -DMBEDTLS_SSL_LIBRARY="$AROS_CRYPTO/lib/libaroscrypto.a" \
    -DMBEDTLS_CRYPTO_LIBRARY="$AROS_CRYPTO/lib/libaroscrypto.a" \
    -DMBEDTLS_X509_LIBRARY="$AROS_CRYPTO/lib/libaroscrypto.a" \
    -DHAVE_GETADDRINFO=ON -DHAVE_SELECT=ON \
    -DHAVE_GCC_THREAD_LOCAL_STORAGE=0 -DHAVE_MSC_THREAD_LOCAL_STORAGE=0 \
    -DHAVE_CONSTRUCTOR_ATTRIBUTE=0 -DHAVE_DESTRUCTOR_ATTRIBUTE=0 \
    -DCMAKE_DISABLE_FIND_PACKAGE_Threads=TRUE -DHAVE_PTHREAD=OFF
cmake --build "$out/libssh" -j "${JOBS:-4}"
version=${LIMPET_VERSION:-0.0.0-dev}
date=${LIMPET_DATE:-$(date +%d.%m.%Y)}
programs='ssh scp ssh-keygen ssh-copy-id'
# compile PROGRAM SOURCES...: one program on the shared part and the libraries.
compile() {
    program=$1; shift
    "$AROS_GCC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Wno-pointer-sign -Wno-deprecated-declarations \
        -Werror=implicit-function-declaration \
        -ffile-prefix-map="$root"=. $define \
        "-DLIMPET_SSH_VERSION=\"$version\"" "-DLIMPET_SSH_DATE=\"$date\"" \
        -I"$source/include" -I"$out/libssh/include" -I"$AROS_CRYPTO/include" "-DMBEDTLS_CONFIG_FILE=<aros_crypto_config.h>" -Isdk/include \
        "$@" examples/ssh/common.c tools/ssh/aros-interface.c tools/ssh/aros-abiv11.c \
        "$out/libssh/src/libssh.a" "$AROS_CRYPTO/lib/libaroscrypto.a" \
        $libs -o "$out/$program"
    found=$("$tool-readelf" -rW "$out/$program")
    if printf '%s\n' "$found" | grep -E "$relocations"; then
        echo "Unsupported AROS loader relocation in $program" >&2
        exit 1
    fi
    if "$tool-nm" -u "$out/$program" | grep .; then
        echo "Undefined symbols in $program" >&2
        exit 1
    fi
}
compile ssh examples/ssh/client.c examples/ssh/stream.c examples/ssh/pump.c examples/ssh/term_query.c \
    examples/ssh/sgr_ansi.c examples/ssh/pens.c sdk/src/limpet_query.c sdk/src/limpet_format.c src/aros_tty_client.c
compile scp examples/ssh/scp.c examples/ssh/scp_spec.c
compile ssh-keygen examples/ssh/keygen.c
compile ssh-copy-id examples/ssh/copy_id.c
# What the package needs beside the programs: the licence texts of what is
# linked into it, and a record of this build.
rm -rf "$out/licences"; mkdir -p "$out/licences"
cp examples/ssh/licenses/libssh-COPYING.txt "$out/licences/libssh-COPYING.txt"
cp examples/ssh/licenses/AROS-APL.txt "$out/licences/AROS-APL.txt"
cp "$AROS_CRYPTO/LICENSE.mbedtls" "$out/licences/MbedTLS-LICENSE.txt"
cp "$AROS_CRYPTO/LICENSE.aros-crypto" "$out/licences/aros-crypto-LICENSE.txt"
if [ -f "$AROS_CRYPTO/LICENSE.jitterentropy.bsd" ]; then
    cp "$AROS_CRYPTO/LICENSE.jitterentropy" "$out/licences/jitterentropy-LICENSE.txt"
    cp "$AROS_CRYPTO/LICENSE.jitterentropy.bsd" "$out/licences/jitterentropy-LICENSE.bsd.txt"
fi
{
    echo "target $target"
    echo "ssh, scp, ssh-keygen, ssh-copy-id $version ($date), libssh 0.12.2"
    sed 's/^/kit: /' "$AROS_CRYPTO/BUILD.txt"
    (cd "$out" && shasum -a 256 $programs)
} > "$out/build-record"
(cd "$out" && ls -l $programs)
