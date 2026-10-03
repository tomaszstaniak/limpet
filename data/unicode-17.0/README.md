# Pinned Unicode width data

Unmodified Unicode 17.0 data, copyright Unicode, Inc., distributed under the
accompanying Unicode-3.0 license:

- https://www.unicode.org/Public/17.0.0/ucd/EastAsianWidth.txt
- https://www.unicode.org/Public/17.0.0/ucd/emoji/emoji-data.txt
- https://www.unicode.org/license.txt

`tools/generate_width_table.py` checks their SHA-256. Regenerate offline:

```sh
python3 tools/generate_width_table.py data/unicode-17.0 src/unicode_width_table.h
```

Policy: EAW W/F or Emoji_Presentation is two columns; otherwise one. Ambiguous
width stays narrow. This is not a complete wcwidth or grapheme algorithm.
Combining characters, variation selectors and ZWJ are not resolved by this table.
Default-wide unassigned ranges explicitly listed upstream are retained.
There is no runtime data download or dependency on the host's Unicode version.
