# Optional application-local fonts

These font files are unmodified. They are not installed into the system.
Copy the entire packaged folder; the application finds them in PROGDIR:Fonts.

## Unifont 17.0.03

- Local filename: `Unifont.otf` (the file is renamed, not its internal font name).
- Source: https://unifoundry.com/pub/unifont/unifont-17.0.03/font-builds/unifont-17.0.03.otf
- SHA-256: `26071c5a97533cefdcbc6b0645e7ee279413049079f09f592b26916ca6c21bf5`
- Source/licensing archive: https://unifoundry.com/pub/unifont/unifont-17.0.03/unifont-17.0.03.tar.gz
- Accompanying original notices: `COPYING` and `OFL-1.1.txt`, extracted unchanged
  from that release. The font files are offered under SIL OFL 1.1 or GPL with
  the font exception; this bundle uses the OFL option. Copyright notices are
  retained in the original font metadata and accompanying distribution notices.
- Used at 16 pixels with a 14-pixel baseline for BMP CJK/Hangul fallback.

## Noto Emoji (monochrome variable font)

- Local filename: `NotoEmoji.ttf`; original filename: `NotoEmoji[wght].ttf`.
- Copyright 2013 Google LLC. License: accompanying `NotoEmoji-OFL.txt` (SIL OFL 1.1).
- Pinned Google Fonts commit: `8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5`.
- Source: https://github.com/google/fonts/blob/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf
- SHA-256: `de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551`
- Used at 12 pixels, default variable-font instance. This is not Noto Color Emoji.

Fallback order is Vera Mono -> Open Sans -> Noto Emoji -> Unifont. The first two
are existing required mainline system fonts. New optional faces can be omitted;
then unsupported characters use the existing missing-glyph fallback. Font metrics
do not override terminal column widths. Coverage is not grapheme composition.
# M2d colour addition

`NotoColorEmoji.ttf` is the unmodified v2.051 font from
https://raw.githubusercontent.com/googlefonts/noto-emoji/v2.051/fonts/NotoColorEmoji.ttf
(SHA-256 `72a635cb3d2f3524c51620cdde406b217204e8a6a06c6a096ff8ed4b5fd6e27b`).
Its original OFL license is `NotoColorEmoji-LICENSE.txt`. It is optional and
application-local. HarfBuzz shapes clusters before FreeType loads the bitmap
strike; the application downsamples into bounded cell rasters. No global font
installation is performed. The earlier M2c choices below remain historical.
