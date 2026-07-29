#!/usr/bin/env python3
"""font_convert.py — render charset glyphs from a TTF/OTF for LithoUI packing.

Can be used as a library (pack_res.py imports render_glyphs) or CLI:

  python tools/font_convert/font_convert.py \\
      --font path/to/NotoSansSC.ttf --size 32 --charset charset.txt

Charset file format:
  # comment
  0020-007E          # inclusive hex range
  4E00               # single codepoint
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import freetype
except ImportError:
    print("ERROR: freetype-py not installed. Run: pip install freetype-py")
    sys.exit(1)


def parse_charset(path: Path) -> list[int]:
    """Parse charset.txt → sorted unique codepoints."""
    cps: set[int] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        # strip trailing inline comment
        if "#" in line:
            line = line[: line.index("#")].strip()
        if not line:
            continue
        if "-" in line:
            a, b = line.split("-", 1)
            lo, hi = int(a.strip(), 16), int(b.strip(), 16)
            if hi < lo:
                lo, hi = hi, lo
            cps.update(range(lo, hi + 1))
        else:
            cps.add(int(line, 16))
    return sorted(cps)


def _crop_bitmap(buf: bytes, pitch: int, rows: int, width: int):
    """Crop empty margins from an 8-bit FreeType bitmap.

    Returns (cropped_bytes_row_major, crop_w, crop_h, trim_left, trim_top).
    Empty ink → (b'', 0, 0, 0, 0).
    """
    if rows <= 0 or width <= 0 or not buf:
        return b"", 0, 0, 0, 0

    # Find bounding box of non-zero pixels
    min_x, min_y = width, rows
    max_x, max_y = -1, -1
    for y in range(rows):
        row = buf[y * pitch : y * pitch + width]
        for x, v in enumerate(row):
            if v:
                if x < min_x:
                    min_x = x
                if x > max_x:
                    max_x = x
                if y < min_y:
                    min_y = y
                if y > max_y:
                    max_y = y

    if max_x < 0:
        return b"", 0, 0, 0, 0

    cw = max_x - min_x + 1
    ch = max_y - min_y + 1
    out = bytearray(cw * ch)
    for y in range(ch):
        src = buf[(min_y + y) * pitch + min_x : (min_y + y) * pitch + min_x + cw]
        out[y * cw : (y + 1) * cw] = src
    return bytes(out), cw, ch, min_x, min_y


def render_glyphs(font_path: Path, pixel_size: int, codepoints: list[int]) -> dict:
    """Render glyphs. Returns dict with font metrics + glyph list.

    Each glyph:
      codepoint, width, height, bearingX, bearingY, advance, gray (list[int] or None)
    gray is row-major A8 of the *cropped* bitmap; None if empty (space etc.).
    """
    face = freetype.Face(str(font_path))
    face.set_pixel_sizes(0, pixel_size)

    # Global metrics in font units → pixels
    # FreeType: ascender/descender are in 26.6 when using size metrics
    ascender = face.size.ascender >> 6
    descender = face.size.descender >> 6  # typically negative
    height = face.size.height >> 6
    if height <= 0:
        height = ascender - descender
    line_height = height if height > 0 else pixel_size

    glyphs = []
    missing = 0
    for cp in codepoints:
        # Always try load; missing glyphs get a zero-ink placeholder with advance of space/em
        flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL
        try:
            face.load_char(cp, flags)
        except freetype.FT_Exception:
            missing += 1
            # Placeholder: empty glyph with em advance
            glyphs.append({
                "codepoint": cp,
                "width": 0,
                "height": 0,
                "bearingX": 0,
                "bearingY": 0,
                "advance": pixel_size // 2,
                "gray": None,
            })
            continue

        slot = face.glyph
        bmp = slot.bitmap
        advance = slot.advance.x >> 6
        bearing_x = slot.bitmap_left
        bearing_y = slot.bitmap_top

        cropped, cw, ch, trim_l, trim_t = _crop_bitmap(
            bytes(bmp.buffer) if bmp.buffer else b"",
            bmp.pitch,
            bmp.rows,
            bmp.width,
        )

        # Adjust bearings for crop (ink moves relative to pen)
        bearing_x += trim_l
        bearing_y -= trim_t

        gray = list(cropped) if cw > 0 and ch > 0 else None
        glyphs.append({
            "codepoint": cp,
            "width": cw,
            "height": ch,
            "bearingX": bearing_x,
            "bearingY": bearing_y,
            "advance": max(0, advance),
            "gray": gray,
        })

    return {
        "pixelSize": pixel_size,
        "ascent": int(ascender),
        "descent": int(descender),
        "lineHeight": int(line_height),
        "glyphs": glyphs,
        "missing": missing,
        "fontPath": str(font_path),
    }


def resolve_font_path(fonts_dir: Path) -> Path | None:
    """Resolve TTF path from fonts/fontpath.txt (first non-comment line)."""
    cfg = fonts_dir / "fontpath.txt"
    if not cfg.exists():
        return None
    for raw in cfg.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "#" in line:
            line = line[: line.index("#")].strip()
        p = Path(line)
        if not p.is_absolute():
            p = (fonts_dir / p).resolve()
        return p
    return None


def main():
    ap = argparse.ArgumentParser(description="Render charset glyphs from a font")
    ap.add_argument("--font", required=True, help="Path to TTF/OTF")
    ap.add_argument("--size", type=int, default=32, help="Pixel size (default 32)")
    ap.add_argument("--charset", required=True, help="charset.txt path")
    args = ap.parse_args()

    cps = parse_charset(Path(args.charset))
    print(f"charset: {len(cps)} codepoints")
    info = render_glyphs(Path(args.font), args.size, cps)
    print(f"font: {info['fontPath']}")
    print(f"  size={info['pixelSize']} ascent={info['ascent']} "
          f"descent={info['descent']} lineHeight={info['lineHeight']}")
    print(f"  glyphs={len(info['glyphs'])} missing={info['missing']}")
    ink = sum(1 for g in info["glyphs"] if g["gray"] is not None)
    bytes_raw = sum(g["width"] * g["height"] for g in info["glyphs"])
    print(f"  with ink={ink}  raw A8 bytes={bytes_raw}")


if __name__ == "__main__":
    main()
