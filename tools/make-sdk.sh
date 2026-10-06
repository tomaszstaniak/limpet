#!/bin/sh
# Build the Limpet SDK archive: dist/sdk/<version>/limpet-sdk-<version>.zip,
# its MANIFEST.sha256 (inside) and SHA256SUMS (beside it).
#
#   ABIV11_GCC=/path/to/x86_64-aros-gcc \
#   ABIV11_SDK=/path/to/SDK/Development \
#   tools/make-sdk.sh
#
# The prebuilt library is compiled from the staged sources, the way a user
# would build it. The script refuses to package if any file holds a host
# path or is of a kind it does not expect. See doc/sdk/releasing.md.
set -eu

cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/^#define LIMPET_SDK_VERSION_STRING "\(.*\)"/\1/p' sdk/include/limpet/limpet.h)
[ -n "$VERSION" ] || { echo "No LIMPET_SDK_VERSION_STRING in sdk/include/limpet/limpet.h"; exit 1; }
: "${ABIV11_GCC:?Set ABIV11_GCC to the x86_64 ABIv11 compiler}"
: "${ABIV11_SDK:?Set ABIV11_SDK to the ABIv11 Developer SDK (its Development directory)}"
[ -d "$ABIV11_SDK/include" ] || { echo "ABIV11_SDK has no include/: $ABIV11_SDK"; exit 1; }
ABIV11_AR=${ABIV11_AR:-${ABIV11_GCC%gcc}ar}
command -v zip >/dev/null || { echo "zip is needed"; exit 1; }
command -v python3 >/dev/null || { echo "python3 is needed for the link rewrite"; exit 1; }

NAME=limpet-sdk-$VERSION
TAG=sdk-v$VERSION
OUT=dist/sdk/$VERSION
STAGE=$OUT/stage/$NAME
rm -rf "$OUT"
mkdir -p "$STAGE"

# The contract tests run against the terminal's own reply code. Never
# package a tree where they fail.
make test-sdk >/dev/null

mkdir -p "$STAGE/include/limpet" "$STAGE/src" "$STAGE/examples" "$STAGE/tests/host/dos" \
         "$STAGE/tests/host/proto" "$STAGE/doc/sdk" "$STAGE/doc/screenshots"
cp sdk/include/limpet/limpet.h sdk/include/limpet/tty_control.h "$STAGE/include/limpet/"
cp sdk/src/limpet_query.c sdk/src/limpet_format.c "$STAGE/src/"
cp sdk/examples/*.c "$STAGE/examples/"
cp sdk/tests/host/dos/*.h "$STAGE/tests/host/dos/"
cp sdk/tests/host/proto/*.h "$STAGE/tests/host/proto/"
cp sdk/Makefile sdk/README.md sdk/CHANGES.md "$STAGE/"
cp LICENSE.txt "$STAGE/LICENSE.txt"
for f in getting-started choosing api examples experimental "verification-$VERSION"; do
    cp "doc/sdk/$f.md" "$STAGE/doc/sdk/"
done
for f in capabilities protocols presentation kitty-graphics hyperlinks compatibility \
         troubleshooting versioning; do
    cp "doc/$f.md" "$STAGE/doc/"
done
cp doc/screenshots/sdk-*.png "$STAGE/doc/screenshots/"

# What lives under sdk/ in the repository is at the top of the archive.
# Any other relative link that leaves the archive points at the same file
# on GitHub, at this release's tag.
python3 - "$STAGE" "$TAG" <<'EOF'
import os, re, sys
stage, tag = sys.argv[1], sys.argv[2]
base = "https://github.com/tomaszstaniak/limpet/blob/%s/" % tag
link = re.compile(r'(\]\()([^)#\s]+)((?:#[^)\s]*)?\))')
for root, _, files in os.walk(os.path.join(stage, "doc")):
    for name in files:
        if not name.endswith(".md"):
            continue
        path = os.path.join(root, name)
        rel_dir = os.path.relpath(root, stage)
        text = open(path, encoding="utf-8").read()
        def fix(m):
            target = m.group(2)
            if re.match(r'[a-z]+:', target):
                return m.group(0)
            repo_path = os.path.normpath(os.path.join(rel_dir, target))
            archive_path = repo_path[len("sdk/"):] if repo_path.startswith("sdk/") else repo_path
            if os.path.exists(os.path.join(stage, archive_path)):
                return m.group(1) + os.path.relpath(os.path.join(stage, archive_path), root) + m.group(3)
            return m.group(1) + base + repo_path + m.group(3)
        open(path, "w", encoding="utf-8").write(link.sub(fix, text))
EOF

make -C "$STAGE" lib CC="$ABIV11_GCC" AR="$ABIV11_AR" AROS_SDK="$ABIV11_SDK" ABI=abiv11 >/dev/null
rm -rf "$STAGE/build"

# Nothing from this machine may travel with the archive.
if grep -rlaE '/Users/|/home/|/private/tmp|/Volumes/' "$STAGE"; then
    echo "Refusing to package: host paths in the files above"; exit 1
fi
UNEXPECTED=$(cd "$STAGE" && find . -type f ! -name '*.c' ! -name '*.h' ! -name '*.md' \
    ! -name Makefile ! -name LICENSE.txt ! -path './doc/screenshots/*.png' \
    ! -path ./lib/abiv11/libLimpet.a)
[ -z "$UNEXPECTED" ] || { echo "Refusing to package unexpected files:"; echo "$UNEXPECTED"; exit 1; }

# Fixed timestamps and order: the same tree gives the same archive.
find "$STAGE" -exec touch -t 202610050000 {} +
(cd "$STAGE" && find . -type f ! -name MANIFEST.sha256 | LC_ALL=C sort | sed 's|^\./||' |
    xargs shasum -a 256 > MANIFEST.sha256)
touch -t 202610050000 "$STAGE/MANIFEST.sha256"
(cd "$OUT/stage" && find "$NAME" -type f | LC_ALL=C sort | zip -X -q "../$NAME.zip" -@)
(cd "$OUT" && shasum -a 256 "$NAME.zip" > SHA256SUMS)
cat "$OUT/SHA256SUMS"
