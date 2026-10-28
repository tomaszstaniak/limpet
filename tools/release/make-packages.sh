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
# The build directories; a test may point this at a copy.
build=${LIMPET_PACKAGE_BUILD:-$root/build}
[[ -e $out ]] && { print -u2 "$out exists"; exit 1; }
# The version is the build's, not this script's: refuse a mismatch.
check_version() {  # $1 = binary, $2 = its name in $VER (default Limpet)
    local v=$(strings $1 | sed -n "s/^\\\$VER: ${2:-Limpet} \\([^ ]*\\) .*/\\1/p" | head -1)
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
    local t=$1 b=$build/$1 want
    case $t in
        aros-x86_64-*) want=x86-64;;
        aros-aarch64-*) want=aarch64;;
        *) print -u2 "unknown target $t"; exit 1;;
    esac
    # The ELF header does not tell the ABIs apart; the C library bases do.
    # ABIv11's remover and LimpetPrefs use only stdlib (StdlibBase), the
    # frontend and worker crt (CrtBase); mainline's all use stdc (StdCBase).
    local libset helperset
    case $t in
        *-abiv11-*) libset=__aros_libset_CrtBase helperset=__aros_libset_StdlibBase;;
        *-abiv1-*) libset=__aros_libset_StdCBase helperset=__aros_libset_StdCBase;;
    esac
    [[ -f $root/tools/release/README-$t.txt ]] || { print -u2 "unknown target $t"; exit 1; }
    for f in $b/Limpet $b/LimpetPrefs $b/limpet-console $b/limpet-handler $b/limpet-handler-remove; do
        file -b $f | grep -q "ELF 64-bit.*$want" ||
            { print -u2 "$f is not an ELF for $t: $(file -b $f)"; exit 1; }
    done
    # grep reads all of nm's output: with pipefail, grep -q stopping at the
    # first match lets nm die of SIGPIPE on a large binary, a false failure.
    for f in $b/Limpet $b/limpet-console; do
        nm $f | grep " $libset\$" >/dev/null || { print -u2 "$f has no $libset: not built for $t"; exit 1; }
    done
    for f in $b/limpet-handler-remove $b/LimpetPrefs; do
        nm $f | grep " $helperset\$" >/dev/null || { print -u2 "$f has no $helperset: not built for $t"; exit 1; }
    done
    # Every binary must be the one make target built for this target: this
    # also covers limpet-handler, which links no C library at all.
    [[ -f $b/build-record ]] || { print -u2 "$b/build-record missing: build with make target TARGET=$t"; exit 1; }
    [[ $(head -1 $b/build-record) == "target $t" ]] || { print -u2 "$b/build-record is for $(head -1 $b/build-record), not $t"; exit 1; }
    (cd $b && tail -n +2 build-record | shasum -a 256 -c --quiet) ||
        { print -u2 "$b: a binary differs from what make target built for $t"; exit 1; }
    check_version $b/Limpet
    check_version $b/LimpetPrefs LimpetPrefs
    local d=$out/stage-$t/Limpet; common $d
    cp $b/Limpet $d/Limpet
    cp $root/assets/icon/Limpet.info.png $d/Limpet.info
    # Project > Preferences starts it; the icon lets Workbench start it too.
    cp $b/LimpetPrefs $d/LimpetPrefs
    cp $root/assets/icon/Limpet.info.png $d/LimpetPrefs.info
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
(( ${#${(u)@}} == $# )) || { print -u2 "a target is named twice"; exit 1; }
for t in $@; do
    if [[ $t == pty ]]; then
        file -b $build/aros-xterm-pty | grep -q "ELF 64-bit.*x86-64" || { print -u2 "build/aros-xterm-pty is not an x86_64 ELF"; exit 1; }
        nm $build/aros-xterm-pty | grep -q " __aros_libset_StdCBase\$" || { print -u2 "build/aros-xterm-pty is not a mainline build"; exit 1; }
        check_version $build/aros-xterm-pty
        # mainline PTY preview
        d=$out/stage-pty/LimpetPTY; common $d
        cp $build/aros-xterm-pty $d/LimpetPTY
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
