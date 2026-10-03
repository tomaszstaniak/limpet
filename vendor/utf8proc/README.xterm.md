# Pinned utf8proc

Upstream: https://github.com/JuliaStrings/utf8proc
Release: v2.11.0 (Unicode 17.0.0).
Archive: https://github.com/JuliaStrings/utf8proc/archive/refs/tags/v2.11.0.tar.gz
Archive SHA-256: `c24379b5fa0a429a1f9a3fc23b44a75f2b141a34c09146a529a55d20a5808070`.

The unmodified source, generated Unicode data, header, license and release notes
are extracted from that archive. Compile `utf8proc.c` with `UTF8PROC_STATIC`;
it includes `utf8proc_data.c` directly. No network is needed during builds.

The screen model calls the stateful UAX #29 extended grapheme API for every
adjacent decoded scalar. Terminal controls, resize and bounded overflow reset
attachment. Width remains a separate terminal policy: existing scalar widths,
VS16 on pictographic/keycap bases, keycaps and pictographic ZWJ promotion.
