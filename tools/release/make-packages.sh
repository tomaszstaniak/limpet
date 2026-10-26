#!/bin/zsh
# Build the release archives from binaries already built by make target.
#   tools/release/make-packages.sh VERSION OUTDIR TARGET... [pty]
# TARGET is one of the Makefile's TARGETS (aros-x86_64-abiv11-pc,
# aros-x86_64-abiv1-pc, aros-aarch64-abiv1-raspi); build/<TARGET>/ holds
# its program, handler and tools. "pty" adds the mainline PTY preview
# (build/aros-xterm-pty). Archives are named
# <Name>-<version>-<target>.zip (AROS-dev docs/coding-standards.md,
# "Target names"). Package contents are fixed here; the binaries are taken
# as built and their hashes recorded, never rebuilt by this script.
set -euo pipefail
ver=$1 out=${2:A}; shift 2
(( $# )) || { print -u2 "name at least one target"; exit 1; }
root=${0:A:h:h:h}
[[ -e $out ]] && { print -u2 "$out exists"; exit 1; }
# The version is the build's, not this script's: refuse a mismatch.
check_version() {  # $1 = binary
    local v=$(strings $1 | sed -n 's/^\$VER: Limpet \([^ ]*\) .*/\1/p' | head -1)
    [[ $v == $ver ]] || { print -u2 "$1 is version '$v', not $ver; rebuild with LIMPET_VERSION=$ver"; exit 1; }
}
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
package_target() {  # $1 = target
    local t=$1 b=$root/build/$1 want
    case $t in
        aros-x86_64-*) want=x86-64;;
        aros-aarch64-*) want=aarch64;;
        *) print -u2 "unknown target $t"; exit 1;;
    esac
    [[ -f $root/tools/release/README-$t.txt ]] || { print -u2 "unknown target $t"; exit 1; }
    for f in $b/Limpet $b/limpet-console $b/limpet-handler $b/limpet-handler-remove; do
        file -b $f | grep -q "ELF 64-bit.*$want" ||
            { print -u2 "$f is not an ELF for $t: $(file -b $f)"; exit 1; }
    done
    check_version $b/Limpet
    local d=$out/stage-$t/Limpet; common $d
    cp $b/Limpet $d/Limpet
    cp $root/assets/icon/Limpet.info.png $d/Limpet.info
    sed "s/@VERSION@/$ver/g" $root/tools/release/README-$t.txt > $d/README.txt
    grep -q '@VERSION@' $d/README.txt && { print -u2 "README still has @VERSION@"; exit 1; }
    # the drawer's own icon, beside it at the archive root (apkg installs it)
    cp $root/assets/icon/Limpet.info.png $out/stage-$t/Limpet.info
    (cd $out/stage-$t && zip -qrX $out/Limpet-$ver-$t.zip Limpet Limpet.info)
    # The experimental LIMPET: console handler: a separate archive that
    # unpacks into the same drawer. The main archive must never carry it.
    local x=$out/stage-handler-$t/Limpet; mkdir -p $x
    cp $b/limpet-handler $b/limpet-console $b/limpet-handler-remove $x/
    check_version $x/limpet-console
    strings $x/limpet-handler | grep -qx limpet-console || { print -u2 "limpet-handler does not look for limpet-console"; exit 1; }
    cp $root/tools/xterm-console/{LIMPET.mountlist,Install-Handler,Remove-Handler} $x/
    sed "s/@VERSION@/$ver/g" $root/tools/xterm-console/README-Handler-$t.txt > $x/README-Handler.txt
    (cd $out/stage-handler-$t && zip -qrX $out/Limpet-handler-experimental-$ver-$t.zip Limpet)
    unzip -Z1 $out/Limpet-$ver-$t.zip | grep -qi 'handler\|limpet-console\|mountlist\|xterm' && { print -u2 "main archive carries handler files"; exit 1; }
    (cd $out/stage-$t && find Limpet -type f | sort | xargs shasum -a 256 > ../contents-$t.sha256)
    (cd $out/stage-handler-$t && find Limpet -type f | sort | xargs shasum -a 256 > ../contents-handler-$t.sha256)
}
for t in $@; do
    if [[ $t == pty ]]; then
        check_version $root/build/aros-xterm-pty
        # mainline PTY preview
        d=$out/stage-pty/LimpetPTY; common $d
        cp $root/build/aros-xterm-pty $d/LimpetPTY
        cp $root/assets/icon/Limpet.info.png $d/LimpetPTY.info
        sed "s/@VERSION@/$ver/g" $root/tools/release/README-pty-preview.txt > $d/README.txt
        (cd $out/stage-pty && zip -qrX $out/Limpet-PTY-$ver-aros-x86_64-abiv1-pc.zip LimpetPTY)
        (cd $out/stage-pty && find LimpetPTY -type f | sort | xargs shasum -a 256 > ../contents-pty.sha256)
    else
        package_target $t
    fi
done
(cd $out && shasum -a 256 *.zip > SHA256SUMS)
cat $out/SHA256SUMS
