#!/bin/zsh
# Build the release-candidate archives from the exact RC binaries.
#   tools/release/make-packages.sh VERSION OUTDIR [abiv11]
# With "abiv11" only the ABIv11 archive is made (and checked).
# Package contents are fixed here; the binaries are taken as built and their
# hashes recorded, never rebuilt by this script.
set -euo pipefail
ver=$1 out=$2 only=${3:-}
root=${0:A:h:h:h}
[[ -e $out ]] && { print -u2 "$out exists"; exit 1; }
# The version is the build's, not this script's: refuse a mismatch.
binaries=($root/build/abiv11/aros-xterm)
[[ $only == abiv11 ]] || binaries+=($root/build/aros-xterm-pty)
for b in $binaries; do
    v=$(strings $b | sed -n 's/^\$VER: Limpet \([^ ]*\) .*/\1/p' | head -1)
    [[ $v == $ver ]] || { print -u2 "$b is version '$v', not $ver; rebuild with LIMPET_VERSION=$ver"; exit 1; }
done
mkdir -p $out
common() {  # $1 = drawer
    mkdir -p $1/Fonts $1/Themes $1/Licenses
    cp $root/assets/fonts/NotoEmoji.ttf $root/assets/fonts/Unifont.otf $1/Fonts/
    cp $root/themes/*.theme $1/Themes/
    cp $root/vendor/harfbuzz/COPYING $1/Licenses/HarfBuzz-COPYING.txt
    cp $root/vendor/utf8proc/LICENSE.md $1/Licenses/utf8proc-LICENSE.md
    cp $root/assets/fonts/COPYING $1/Licenses/Unifont-COPYING.txt
    cp $root/assets/fonts/OFL-1.1.txt $1/Licenses/Unifont-OFL-1.1.txt
    cp $root/assets/fonts/NotoEmoji-OFL.txt $1/Licenses/NotoEmoji-OFL.txt
    cp $root/data/unicode-17.0/LICENSE.txt $1/Licenses/Unicode-3.0.txt
    cp $root/LICENSE.txt $1/LICENSE.txt
}
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
unzip -l $out/Limpet-$ver-abiv11.zip | grep -qi 'handler\|limpet-console\|mountlist\|xterm' && { print -u2 "main archive carries handler files"; exit 1; }
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
