#!/bin/zsh
# Build the release-candidate archives from the exact RC binaries.
#   tools/release/make-packages.sh VERSION OUTDIR [abiv11|v1-x86_64|v1-aarch64]
# With "abiv11" only the ABIv11 archive is made (and checked); with a
# v1 target, that target's program and handler archives (make target).
# Package contents are fixed here; the binaries are taken as built and their
# hashes recorded, never rebuilt by this script.
set -euo pipefail
ver=$1 out=${2:A} only=${3:-}
root=${0:A:h:h:h}
[[ -e $out ]] && { print -u2 "$out exists"; exit 1; }
# The version is the build's, not this script's: refuse a mismatch.
binaries=()
[[ $only == v1-* ]] || binaries=($root/build/abiv11/aros-xterm)
[[ $only == abiv11 || $only == v1-* ]] || binaries+=($root/build/aros-xterm-pty)
for b in $binaries; do
    v=$(strings $b | sed -n 's/^\$VER: Limpet \([^ ]*\) .*/\1/p' | head -1)
    [[ $v == $ver ]] || { print -u2 "$b is version '$v', not $ver; rebuild with LIMPET_VERSION=$ver"; exit 1; }
done
mkdir -p $out
common() {  # $1 = drawer
    mkdir -p $1/Fonts $1/Themes $1/Licenses
    cp $root/assets/fonts/NotoEmoji.ttf $root/assets/fonts/Unifont.otf $1/Fonts/
    # The primary font when present: complete eza icon repertoire in one cell
    # (XTY_FEATURE_NERD_ICONS_EZA/ICON_WIDTH_ONE are offered only with it).
    cp $root/assets/fonts/DejaVuSansMNerdFontMono-Regular.ttf $1/Fonts/
    cp $root/assets/fonts/DejaVu-NerdFont-LICENSE.txt $1/Licenses/DejaVuSansM-NerdFont-LICENSE.txt
    cp $root/assets/fonts/NerdFonts-LICENSE.md $1/Licenses/NerdFonts-LICENSE.md
    cp $root/themes/*.theme $1/Themes/
    cp $root/vendor/harfbuzz/COPYING $1/Licenses/HarfBuzz-COPYING.txt
    cp $root/vendor/utf8proc/LICENSE.md $1/Licenses/utf8proc-LICENSE.md
    cp $root/assets/fonts/COPYING $1/Licenses/Unifont-COPYING.txt
    cp $root/assets/fonts/OFL-1.1.txt $1/Licenses/Unifont-OFL-1.1.txt
    cp $root/assets/fonts/NotoEmoji-OFL.txt $1/Licenses/NotoEmoji-OFL.txt
    cp $root/data/unicode-17.0/LICENSE.txt $1/Licenses/Unicode-3.0.txt
    cp $root/LICENSE.txt $1/LICENSE.txt
}
# Another ABI or CPU (mk/target-<name>.mk): build/<name>/ holds the program,
# the handler and its tools. The archive names carry the target.
if [[ $only == v1-* ]]; then
    t=$only b=$root/build/$only
    case $t in v1-x86_64) want=x86-64;; v1-aarch64) want=aarch64;; *) print -u2 "unknown target $t"; exit 1;; esac
    for f in $b/Limpet $b/limpet-console $b/limpet-handler $b/limpet-handler-remove; do
        file -b $f | grep -q "ELF 64-bit.*$want" ||
            { print -u2 "$f is not an ELF for $t: $(file -b $f)"; exit 1; }
    done
    v=$(strings $b/Limpet | sed -n 's/^\$VER: Limpet \([^ ]*\) .*/\1/p' | head -1)
    [[ $v == $ver ]] || { print -u2 "$b/Limpet is version '$v', not $ver"; exit 1; }
    d=$out/stage-$t/Limpet; common $d
    cp $b/Limpet $d/Limpet
    cp $root/assets/icon/Limpet.info.png $d/Limpet.info
    sed "s/@VERSION@/$ver/g" $root/tools/release/README-$t.txt > $d/README.txt
    grep -q '@VERSION@' $d/README.txt && { print -u2 "README still has @VERSION@"; exit 1; }
    cp $root/assets/icon/Limpet.info.png $out/stage-$t/Limpet.info
    (cd $out/stage-$t && zip -qrX $out/Limpet-$ver-$t.zip Limpet Limpet.info)
    x=$out/stage-handler-$t/Limpet; mkdir -p $x
    cp $b/limpet-handler $b/limpet-console $b/limpet-handler-remove $x/
    v=$(strings $x/limpet-console | sed -n 's/^\$VER: Limpet \([^ ]*\) .*/\1/p' | head -1)
    [[ $v == $ver ]] || { print -u2 "the worker is version '$v', not $ver"; exit 1; }
    strings $x/limpet-handler | grep -qx limpet-console || { print -u2 "limpet-handler does not look for limpet-console"; exit 1; }
    cp $root/tools/xterm-console/{LIMPET.mountlist,Install-Handler,Remove-Handler} $x/
    sed "s/@VERSION@/$ver/g" $root/tools/xterm-console/README-Handler-$t.txt > $x/README-Handler.txt
    (cd $out/stage-handler-$t && zip -qrX $out/Limpet-handler-experimental-$ver-$t.zip Limpet)
    unzip -Z1 $out/Limpet-$ver-$t.zip | grep -qi 'handler\|limpet-console\|mountlist' && { print -u2 "main archive carries handler files"; exit 1; }
    # Own name: the release's SHA256SUMS gets these lines added to it.
    (cd $out && shasum -a 256 *.zip > SHA256SUMS-$t
     (cd stage-$t && find Limpet -type f | sort | xargs shasum -a 256 > ../contents-$t.sha256)
     (cd stage-handler-$t && find Limpet -type f | sort | xargs shasum -a 256 > ../contents-handler-$t.sha256))
    cat $out/SHA256SUMS-$t
    exit 0
fi
# ABIv11 package
d=$out/stage-abiv11/Limpet; common $d
cp $root/build/abiv11/aros-xterm $d/Limpet
cp $root/assets/icon/Limpet.info.png $d/Limpet.info
sed "s/@VERSION@/$ver/g" $root/tools/release/README-abiv11.txt > $d/README.txt
grep -q 'rc[0-9]\|@VERSION@' $d/README.txt && [[ $ver != *rc* ]] && { print -u2 "README still names an rc"; exit 1; }
# the drawer's own icon, beside it at the archive root (apkg installs it)
cp $root/assets/icon/Limpet.info.png $out/stage-abiv11/Limpet.info
(cd $out/stage-abiv11 && zip -qrX $out/Limpet-$ver-abiv11.zip Limpet Limpet.info)
# The experimental LIMPET: console handler: a separate archive that unpacks
# into the same drawer. The main archive must never carry the handler.
x=$out/stage-handler/Limpet; mkdir -p $x
cp $root/build/abiv11/limpet-handler $x/limpet-handler
cp $root/build/abiv11/aros-xterm-worker $x/limpet-console
cp $root/build/abiv11/xterm-remove $x/limpet-handler-remove
v=$(strings $x/limpet-console | sed -n 's/^\$VER: Limpet \([^ ]*\) .*/\1/p' | head -1)
[[ $v == $ver ]] || { print -u2 "the worker is version '$v', not $ver"; exit 1; }
strings $x/limpet-handler | grep -qx limpet-console || { print -u2 "limpet-handler does not look for limpet-console"; exit 1; }
cp $root/tools/xterm-console/{LIMPET.mountlist,Install-Handler,Remove-Handler} $x/
sed "s/@VERSION@/$ver/g" $root/tools/xterm-console/README-Handler.txt > $x/README-Handler.txt
(cd $out/stage-handler && zip -qrX $out/Limpet-handler-experimental-$ver-abiv11.zip Limpet)
unzip -Z1 $out/Limpet-$ver-abiv11.zip | grep -qi 'handler\|limpet-console\|mountlist\|xterm' && { print -u2 "main archive carries handler files"; exit 1; }
if [[ $only != abiv11 ]]; then
# mainline PTY preview
d=$out/stage-pty/LimpetPTY; common $d
cp $root/build/aros-xterm-pty $d/LimpetPTY
cp $root/assets/icon/Limpet.info.png $d/LimpetPTY.info
sed "s/@VERSION@/$ver/g" $root/tools/release/README-pty-preview.txt > $d/README.txt
(cd $out/stage-pty && zip -qrX $out/Limpet-PTY-$ver-mainline.zip LimpetPTY)
fi
(cd $out && shasum -a 256 *.zip > SHA256SUMS
 (cd stage-abiv11 && find Limpet -type f | sort | xargs shasum -a 256 > ../contents-abiv11.sha256)
 (cd stage-handler && find Limpet -type f | sort | xargs shasum -a 256 > ../contents-handler.sha256)
 [[ ! -d stage-pty ]] || (cd stage-pty && find LimpetPTY -type f | sort | xargs shasum -a 256 > ../contents-pty.sha256))
cat $out/SHA256SUMS
