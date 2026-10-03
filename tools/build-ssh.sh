#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
AROS_GCC=${AROS_GCC:-x86_64-aros-gcc}
AROS_SDK=${AROS_SDK:?set AROS_SDK to the mainline SDK}
root=$(pwd)
archive=build/ssh/libssh-0.12.2.tar.xz
mkdir -p build/ssh
if [ ! -f "$archive" ]; then
    curl -fL --proto '=https' https://www.libssh.org/files/0.12/libssh-0.12.2.tar.xz -o "$archive.part"
    actual=$(shasum -a 256 "$archive.part" | cut -d ' ' -f 1)
    test "$actual" = 49560f677d96e3706a904ac2de1116e25f3680937d51e5c92198fcba4a1c1e9f
    mv "$archive.part" "$archive"
fi
actual=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
test "$actual" = 49560f677d96e3706a904ac2de1116e25f3680937d51e5c92198fcba4a1c1e9f
if [ ! -d build/ssh/libssh-0.12.2 ]; then
    tar -xf "$archive" -C build/ssh
fi
if ! patch -s -R --dry-run -d build/ssh/libssh-0.12.2 -p1 < tools/ssh/libssh-aros.patch; then
    patch --dry-run -d build/ssh/libssh-0.12.2 -p1 < tools/ssh/libssh-aros.patch
    patch -d build/ssh/libssh-0.12.2 -p1 < tools/ssh/libssh-aros.patch
fi
cmake -S build/ssh/libssh-0.12.2 -B build/ssh/mainline \
    -DCMAKE_PROJECT_libssh_INCLUDE="$root/tools/ssh/aros-cmake.cmake" \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
    -DCMAKE_C_COMPILER="$AROS_GCC" \
    -DCMAKE_AR="${AROS_GCC%gcc}ar" -DCMAKE_RANLIB="${AROS_GCC%gcc}ranlib" \
    -DCMAKE_SYSROOT="$AROS_SDK" -DCMAKE_FIND_ROOT_PATH="$AROS_SDK" \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_C_FLAGS="-include \"$root/tools/ssh/aros-platform.h\" -Werror=implicit-function-declaration -Werror=int-conversion" \
    -DCMAKE_EXE_LINKER_FLAGS='-Wl,-u,SocketBase -lnet' \
    -DBUILD_SHARED_LIBS=OFF -DWITH_EXAMPLES=OFF -DWITH_GSSAPI=OFF \
    -DWITH_ZLIB=OFF -DWITH_SERVER=OFF -DWITH_SFTP=OFF -DWITH_NACL=OFF \
    -DWITH_EXEC=OFF -DWITH_PCAP=OFF -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR="$AROS_SDK" -DHAVE_GETADDRINFO=ON -DHAVE_SELECT=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_Threads=TRUE -DHAVE_PTHREAD=OFF \
    -U 'HAVE_OPENSSL_EVP_KDF_CTX_NEW*' -U HAVE_OPENSSL_RAND_PRIV_BYTES
cmake --build build/ssh/mainline -j "${JOBS:-4}"
"$AROS_GCC" --sysroot="$AROS_SDK" -std=gnu99 \
    -Ibuild/ssh/libssh-0.12.2/include -Ibuild/ssh/mainline/include \
    examples/ssh/client.c examples/ssh/stream.c examples/ssh/pump.c examples/ssh/term_query.c src/aros_tty_client.c \
    tools/ssh/aros-interface.c build/ssh/mainline/src/libssh.a \
    -lcrypto -lnet -lposixc -lstdcio -lstdc -lautoinit -o build/ssh/ssh
"$AROS_GCC" --sysroot="$AROS_SDK" -std=gnu99 -Wall -Wextra -Werror \
    examples/ssh/check-transfer.c -o build/ssh/check-transfer
relocations=$("${AROS_GCC%gcc}readelf" -rW build/ssh/ssh)
if printf '%s\n' "$relocations" | rg 'R_X86_64_[^[:space:]]*(GOT|TLS|TPOFF)'; then
    echo 'Unsupported AROS loader relocation in SSH executable' >&2
    exit 1
fi
