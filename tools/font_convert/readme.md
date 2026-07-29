# font_convert

Renders a charset-driven glyph set from a TTF/OTF for LithoUI packing.

Normally invoked by `components/lithoui/tools/pack_res.py` when the UI tree
contains a `fonts/` directory (`charset.txt` + `fontpath.txt`).

## Standalone

```bash
pip install freetype-py Pillow
python tools/font_convert/font_convert.py \
  --font path/to/NotoSansSC.ttf --size 32 \
  --charset components/lithoui/ui/hello_litho/fonts/charset.txt
```

## Config

- `fonts/charset.txt` — codepoints / ranges to pack
- `fonts/fontpath.txt` — path to TTF (not checked in)

Glyphs are stored as `FMT_A8_RLE` with real cropped width/height plus
bearingX/bearingY/advance for layout.
