#!/usr/bin/env python3
"""pack_res.py — pack typed UI resources into res_images.bin + headers.

Usage:  python3 tools/pack_res.py <ui_dir> [output_dir]

Directory layout:
    ui/<name>/
      info.txt              name=..., version=...
      tint/                 grayscale -> FMT_A8_RLE (tintable at runtime)
      solid/                opaque color -> FMT_PAL_RLE or FMT_RGB565_RLE
      alpha/                color + alpha -> FMT_PAL_ALPHA_RLE or FMT_RGB565A_RLE
      fonts/                optional charset-driven font section
        charset.txt         unicode list / ranges
        fontpath.txt        path to TTF/OTF (not committed)

Outputs:
    res_images.bin    binary bundle (images + optional LFNT font section)
    res_images.h      ImageId enum, ImageEntry, font accessors
                      (sin/cos live in core/sin_table.hpp — not in the bin)

Any image can be rotated at runtime via ImageView / Painter; there is no
separate "rotatable" pack path.
"""

import os, struct, sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)

# Allow importing tools/font_convert/font_convert.py
_REPO_ROOT = Path(__file__).resolve().parents[3]
if str(_REPO_ROOT / "tools" / "font_convert") not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT / "tools" / "font_convert"))

# ── globals (set in main) ──

GEN_DIR    = None
BUNDLE_NAME = None

# ── constants ──

MAGIC       = b"LIMB"
VERSION     = 0x00030000
ENTRY_SIZE  = 16
HEADER_SIZE = 16  # magic + version + count + flags + fontsOffset

FONT_MAGIC        = b"LFNT"
FONT_SECTION_HDR  = 16
GLYPH_ENTRY_SIZE  = 24
DEFAULT_FONT_SIZE = 32

# Format enum — 5 formats (all RLE)
FMT_A8_RLE           = 0  # grayscale RLE, tint coloring, opaque
FMT_PAL_RLE          = 1  # palette RLE, RGB565 palette, opaque
FMT_PAL_ALPHA_RLE    = 2  # palette RLE, RGB565 palette, alpha inline in RLE stream
FMT_RGB565_RLE       = 3  # direct color RLE, opaque
FMT_RGB565A_RLE      = 4  # direct color RLE, alpha inline in RLE stream

def make_format_info(fmt, palette_bits):
    """Pack format + paletteBits into one byte.
    bits 2:0 = format enum (0-4)
    bits 7:3 = paletteBits: 0=no palette, 1..8 = log2 of palette entry count
    """
    return (palette_bits << 3) | fmt

# Alpha encoding for non-opaque runs (bit7=0 → TT in bits 6:5):
#   TT=00 → α=0   (fully transparent, no color data)
#   TT=01 → α=85  (semi-transparent level 1)
#   TT=10 → α=170 (semi-transparent level 2)
#   TT=11 → α=213 (semi-transparent level 3, NOT 255 — 255 uses bit7=1)
NONOPAQUE_ALPHA_LEVELS = [0, 85, 170, 213]
OPAQUE_THRESHOLD = 234  # α >= this → bit7=1 (fully opaque, 7-bit run length)

FLAG_HAS_FONTS = 0x0001

# Directory prefixes
PRE_OPAQUE = ""
PRE_ALPHA  = "A_"
PRE_GRAY   = "G_"


# ── helpers ──

def rgba_to_rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def safe_enum_name(name):
    return name.replace("-", "_").replace(" ", "_").upper()


def luminance(r, g, b):
    """BT.601 luminance, 0..255."""
    return (r * 77 + g * 150 + b * 29) // 256


def quantize_alpha_nonopaque(a):
    """Quantize alpha for non-opaque encoding (bit7=0). Returns TT value (0..3).
    Thresholds are midpoints between the 4 non-opaque alpha levels (0, 85, 170, 213)."""
    if a >= 191: return 3       # α=213
    if a >= 128: return 2       # α=170
    if a >= 43:  return 1       # α=85
    return 0                     # α=0

def is_opaque_alpha(a):
    """Check if alpha qualifies for opaque encoding (bit7=1, α=255)."""
    return a >= OPAQUE_THRESHOLD


# ── unified RLE encoder ─────────────────────────────────────────────

def encode_rle(values, w, h, alpha=None):
    """
    Encode a row-major array of bytes into RLE format.

    No-alpha formats (alpha=None, FMT_A8_RLE / FMT_PAL_RLE):
      [value_byte][length_byte]  — length = count-1 (0..255, 1..256 pixels)

    Alpha format (FMT_PAL_ALPHA_RLE) — variable-length head byte:
      bit7=1 (opaque, α=255):
        [1|LLLLLLL]  LLLLLLL = run_len-1 (0..127 → 1..128 pixels)
        Followed by [palette_idx]
      bit7=0 (non-opaque):
        [0|TT|LLLLL]  TT=bits6:5 (alpha level), LLLLL=bits4:0=run_len-1 (0..31 → 1..32)
        TT=00 (α=0):   1-byte record, no color data
        TT=01 (α=85):  head + [palette_idx]
        TT=10 (α=170): head + [palette_idx]
        TT=11 (α=213): head + [palette_idx]

    Returns bytes: [h*4 offset table][RLE stream].
    """
    out = bytearray(h * 4)  # placeholder for offset table
    max_run_no_alpha = 256   # full 8-bit length for no-alpha formats
    max_run_opaque   = 128   # max run for opaque alpha (7-bit length)
    max_run_alpha    = 32    # max run for non-opaque alpha (5-bit length)
    ROW_RAW_FLAG     = 0x80000000

    for y in range(h):
        row = values[y * w:(y + 1) * w]
        row_alpha = alpha[y * w:(y + 1) * w] if alpha else None

        row_start = len(out)  # where row data begins

        x = 0
        while x < w:
            v = row[x]

            if row_alpha:
                # ── PAL_ALPHA_RLE (new variable-length encoding) ──
                a_val = row_alpha[x]
                opaque = is_opaque_alpha(a_val)

                if opaque:
                    # bit7=1: fully opaque, α=255, 7-bit run length
                    run = 1
                    while (x + run < w and run < max_run_opaque and
                           row[x + run] == v and is_opaque_alpha(row_alpha[x + run])):
                        run += 1
                    out.append(0x80 | (run - 1))
                    out.append(v)
                else:
                    # bit7=0: non-opaque, TT + 5-bit run length
                    tt = quantize_alpha_nonopaque(a_val)
                    run = 1
                    while (x + run < w and run < max_run_alpha and
                           row[x + run] == v and
                           not is_opaque_alpha(row_alpha[x + run]) and
                           quantize_alpha_nonopaque(row_alpha[x + run]) == tt):
                        run += 1
                    if tt == 0:
                        # α=0: fully transparent, no color data
                        out.append(run - 1)  # TT=00 in bits 6:5
                    else:
                        # TT=01→α=85, TT=10→α=170, TT=11→α=213
                        out.append((tt << 5) | (run - 1))
                        out.append(v)
                x += run
            else:
                # ── A8_RLE / PAL_RLE (no alpha): per-row adaptive ─
                run = 1
                while x + run < w and run < max_run_no_alpha and row[x + run] == v:
                    run += 1
                out.append(v)
                out.append(run - 1)  # full 8-bit: 0..255
                x += run

        # Per-row adaptive: if raw (1B/px) is smaller than RLE, use raw
        if not row_alpha:
            rle_bytes = len(out) - row_start
            if w < rle_bytes:
                del out[row_start:]          # undo RLE
                for v in row:
                    out.append(v)            # raw 1B/px
                struct.pack_into('<I', out, y * 4, row_start | ROW_RAW_FLAG)
            else:
                struct.pack_into('<I', out, y * 4, row_start)
        else:
            struct.pack_into('<I', out, y * 4, row_start)

    return bytes(out)


def rle_encode_rgb565(pixels, w, h):
    """
    RLE compress RGB565 uint16 pixels with row offset table.
    cmd byte: bit7=0→run(count-1)[color×2]; bit7=1→literal(count-1)[pixels×2n]
    Per-row adaptive: offset bit31=1 → raw RGB565 pixels (w*2 bytes).
    Returns bytes: [h*4 off][RLE/raw stream], or None if entirely RLE > raw.
    """
    ROW_RAW_FLAG = 0x80000000
    off_size = h * 4
    out = bytearray(off_size)
    raw_row_bytes = w * 2
    for y in range(h):
        row = pixels[y * w:(y + 1) * w]
        row_start = len(out)
        x = 0
        while x < w:
            c = row[x]
            run = 1
            while x + run < w and run < 128 and row[x + run] == c:
                run += 1
            if run >= 2:
                out.append(run - 1)
                out.append(c & 0xFF)
                out.append((c >> 8) & 0xFF)
                x += run
            else:
                lit = 1
                while x + lit < w and lit < 128:
                    if x + lit + 1 < w and row[x + lit + 1] == row[x + lit]:
                        break
                    lit += 1
                out.append(0x80 | (lit - 1))
                for k in range(lit):
                    pc = row[x + k]
                    out.append(pc & 0xFF)
                    out.append((pc >> 8) & 0xFF)
                x += lit

        # Per-row adaptive: raw = 2B/px
        rle_bytes = len(out) - row_start
        if raw_row_bytes < rle_bytes:
            del out[row_start:]
            for c in row:
                out.append(c & 0xFF)
                out.append((c >> 8) & 0xFF)
            struct.pack_into('<I', out, y * 4, row_start | ROW_RAW_FLAG)
        else:
            struct.pack_into('<I', out, y * 4, row_start)

    return bytes(out) if len(out) < w * h * 2 else None


def encode_rle_rgb565_alpha(pixels, alphas, w, h):
    """
    RLE for FMT_RGB565A_RLE. Row offset table + variable-length records.

    Record format (variable-length head byte):
      bit7=1 (opaque, α=255):
        [1|LLLLLLL]  LLLLLLL = run_len-1 (0..127 → 1..128 pixels)
        Followed by [c_lo][c_hi]
      bit7=0 (non-opaque):
        [0|TT|LLLLL]  TT=bits6:5 (alpha level), LLLLL=bits4:0=run_len-1 (0..31 → 1..32)
        TT=00 (α=0):   1-byte record, no color data
        TT=01 (α=85):  head + [c_lo][c_hi]
        TT=10 (α=170): head + [c_lo][c_hi]
        TT=11 (α=213): head + [c_lo][c_hi]

    Returns bytes: [h*4 off][RLE stream].
    """
    out = bytearray(h * 4)
    max_run_opaque = 128
    max_run_alpha  = 32

    for y in range(h):
        struct.pack_into('<I', out, y * 4, len(out))
        row_pix = pixels[y * w:(y + 1) * w]
        row_a   = alphas[y * w:(y + 1) * w]
        x = 0
        while x < w:
            c = row_pix[x]
            a_val = row_a[x]
            opaque = is_opaque_alpha(a_val)

            if opaque:
                run = 1
                while (x + run < w and run < max_run_opaque and
                       row_pix[x + run] == c and
                       is_opaque_alpha(row_a[x + run])):
                    run += 1
                out.append(0x80 | (run - 1))
                out.append(c & 0xFF)
                out.append((c >> 8) & 0xFF)
            else:
                tt = quantize_alpha_nonopaque(a_val)
                run = 1
                while (x + run < w and run < max_run_alpha and
                       row_pix[x + run] == c and
                       not is_opaque_alpha(row_a[x + run]) and
                       quantize_alpha_nonopaque(row_a[x + run]) == tt):
                    run += 1
                if tt == 0:
                    out.append(run - 1)
                else:
                    out.append((tt << 5) | (run - 1))
                    out.append(c & 0xFF)
                    out.append((c >> 8) & 0xFF)
            x += run

    return bytes(out)


# ── pack functions ──

def pack_grayscale(path):
    """PNG → grayscale values (0..255)."""
    img = Image.open(path).convert("RGB")
    w, h = img.size
    data = img.getdata()
    gray = [luminance(r, g, b) for r, g, b in data]
    return w, h, gray


def round_up_pow2(n):
    """Round n up to the nearest power of 2."""
    if n <= 2: return 2
    return 1 << (n - 1).bit_length()


def pack_pal8(path, with_alpha=False):
    """
    PNG → dynamic-size palette + index array + optional alpha array.

    Returns (w, h, pal565, pal_bits, idx, alpha_or_None).
    pal_bits = log2(palette entry count), 1..8.
    """
    if with_alpha:
        img = Image.open(path).convert("RGBA")
        w, h = img.size
        data = list(img.getdata())
        rgb_data = [(r, g, b) for r, g, b, a in data]
        alpha_data = [a for r, g, b, a in data]
    else:
        img = Image.open(path)
        if img.mode == 'RGBA':
            # Composite against black to bake alpha into RGB values.
            # Without this, semi-transparent edge pixels keep their full
            # foreground colour after .convert("RGB") and produce hard
            # aliased edges after opaque quantisation.
            bg = Image.new('RGBA', img.size, (0, 0, 0, 255))
            img = Image.alpha_composite(bg, img).convert("RGB")
        else:
            img = img.convert("RGB")
        w, h = img.size
        rgb_data = list(img.getdata())
        alpha_data = None

    # Quantize RGB to max 256 colors
    tmp = Image.new("RGB", (w, h))
    tmp.putdata(rgb_data)
    q = tmp.quantize(colors=256, method=Image.MEDIANCUT)
    idx = list(q.getdata())
    pal = q.getpalette() or []

    # Dynamically size the palette to the actual used colors
    max_idx = max(idx) + 1  # actual color count used in the index
    ncol = round_up_pow2(max_idx)
    if ncol > 256: ncol = 256
    pal_bits = ncol.bit_length() - 1  # log2(palette size), 1..8
    pal565 = [rgba_to_rgb565(pal[i*3], pal[i*3+1], pal[i*3+2]) for i in range(min(ncol, len(pal) // 3))]

    return w, h, pal565, pal_bits, idx, alpha_data


def has_meaningful_alpha(alpha_data):
    """Check if any alpha value is significantly non-opaque."""
    if not alpha_data:
        return False
    for a in alpha_data:
        if a < 240:  # threshold for "meaningfully transparent"
            return True
    return False


# ── font packing ──

def build_font_section(fonts_dir: Path):
    """Render + encode font section. Returns dict or None."""
    charset_path = fonts_dir / "charset.txt"
    if not charset_path.exists():
        return None

    from font_convert import parse_charset, render_glyphs, resolve_font_path

    font_path = resolve_font_path(fonts_dir)
    if font_path is None or not font_path.exists():
        print("ERROR: fonts/ present but font TTF not found.")
        print(f"  Set a path in {fonts_dir / 'fontpath.txt'}")
        sys.exit(1)

    cps = parse_charset(charset_path)
    if not cps:
        print("WARNING: charset.txt empty — skipping font section")
        return None

    print(f"\n  [fonts/]  {len(cps)} codepoints  size={DEFAULT_FONT_SIZE}px")
    print(f"    font: {font_path}")
    info = render_glyphs(font_path, DEFAULT_FONT_SIZE, cps)
    print(f"    ascent={info['ascent']} descent={info['descent']} "
          f"lineHeight={info['lineHeight']} missing={info['missing']}")

    glyphs_out = []
    total_rle = 0
    for g in info["glyphs"]:
        w, h = g["width"], g["height"]
        if w > 0 and h > 0 and g["gray"] is not None:
            chunk = encode_rle(g["gray"], w, h)
        else:
            chunk = b""
        total_rle += len(chunk)
        glyphs_out.append({
            "codepoint": g["codepoint"],
            "width": w,
            "height": h,
            "bearingX": g["bearingX"],
            "bearingY": g["bearingY"],
            "advance": g["advance"],
            "formatInfo": make_format_info(FMT_A8_RLE, 0),
            "chunk": chunk,
            "offset": 0,
            "size": 0,
        })

    print(f"    glyphs RLE total: {total_rle} bytes")
    return {
        "pixelSize": info["pixelSize"],
        "ascent": info["ascent"],
        "descent": info["descent"],
        "lineHeight": info["lineHeight"],
        "glyphs": glyphs_out,
    }


def serialize_font_section(font_info, fonts_offset: int) -> bytes:
    """Serialize Font section; assign absolute glyph chunk offsets."""
    glyphs = font_info["glyphs"]
    n = len(glyphs)
    table_bytes = FONT_SECTION_HDR + n * GLYPH_ENTRY_SIZE
    off = fonts_offset + table_bytes
    for g in glyphs:
        g["offset"] = off
        g["size"] = len(g["chunk"])
        off += len(g["chunk"])

    out = bytearray()
    out += FONT_MAGIC
    out += struct.pack("<H", font_info["pixelSize"] & 0xFFFF)
    out += struct.pack("<h", int(font_info["ascent"]))
    out += struct.pack("<h", int(font_info["descent"]))
    out += struct.pack("<H", font_info["lineHeight"] & 0xFFFF)
    out += struct.pack("<H", n & 0xFFFF)
    out += struct.pack("<H", 0)

    for g in glyphs:
        # GlyphEntry 24B
        out += struct.pack(
            "<IHHhhHBBII",
            g["codepoint"] & 0xFFFFFFFF,
            g["width"] & 0xFFFF,
            g["height"] & 0xFFFF,
            int(g["bearingX"]),
            int(g["bearingY"]),
            g["advance"] & 0xFFFF,
            g["formatInfo"] & 0xFF,
            0,
            g["offset"] & 0xFFFFFFFF,
            g["size"] & 0xFFFFFFFF,
        )

    for g in glyphs:
        out += g["chunk"]

    assert len(out) == (off - fonts_offset)
    return bytes(out)


# ── binary writer ──

def write_bin(path, entries, font_info=None):
    """Write res_images.bin (images → fonts). Sin table lives in code."""
    has_fonts = font_info is not None and len(font_info["glyphs"]) > 0
    flags = 0
    if has_fonts:
        flags |= FLAG_HAS_FONTS

    off = HEADER_SIZE + len(entries) * ENTRY_SIZE
    for e in entries:
        e["offset"] = off
        e["size"] = len(e["chunk"])
        off += len(e["chunk"])

    fonts_offset = 0
    font_blob = b""
    if has_fonts:
        fonts_offset = off
        font_blob = serialize_font_section(font_info, fonts_offset)
        off += len(font_blob)

    with open(path, "wb") as f:
        # Header (16 bytes)
        f.write(MAGIC)
        f.write(struct.pack("<I", VERSION))
        f.write(struct.pack("<H", len(entries)))
        f.write(struct.pack("<H", flags))
        f.write(struct.pack("<I", fonts_offset))

        for e in entries:
            f.write(struct.pack("<HHHBBII",
                     e["id"], e["width"], e["height"],
                     e["formatInfo"], 0,
                     e["offset"], e["size"]))

        for e in entries:
            f.write(e["chunk"])

        if font_blob:
            f.write(font_blob)


# ── header writer ──

RES_H_TEMPLATE = """// Auto-generated by pack_res.py — DO NOT EDIT
#pragma once
#include <stdint.h>

#define RES_BUNDLE_NAME    "{bundle_name}"
#define RES_BUNDLE_VERSION 0x{version:08X}

#define RES_FLAG_HAS_FONTS 0x0001

typedef enum ImageId {{
{enum_entries}
    IMG_COUNT = {count}
}} ImageId;

enum ImageFormat {{
    FMT_A8_RLE          = 0,  // grayscale RLE, tint coloring, opaque
    FMT_PAL_RLE         = 1,  // palette RLE, RGB565 palette, opaque
    FMT_PAL_ALPHA_RLE   = 2,  // palette RLE, RGB565 palette, alpha inline (variable-length head)
    FMT_RGB565_RLE      = 3,  // direct color RLE, opaque
    FMT_RGB565A_RLE     = 4,  // direct color RLE, alpha inline (variable-length head)
}};

// formatInfo byte: bits 2:0 = format enum, bits 7:3 = paletteBits
// paletteBits = 0 → no palette; 1..8 → 2^N palette entries (RGB565 each)
#define LITHO_FORMAT(info)       ((info) & 0x07)
#define LITHO_PALETTE_BITS(info) (((info) >> 3) & 0x1F)
#define LITHO_PALETTE_SIZE(info) (LITHO_PALETTE_BITS(info) ? (1 << LITHO_PALETTE_BITS(info)) : 0)

#pragma pack(push, 1)
typedef struct ImageEntry {{
    uint16_t id;
    uint16_t width;
    uint16_t height;
    uint8_t  formatInfo;   // format + paletteBits (see macros above)
    uint8_t  reserved;
    uint32_t offset;
    uint32_t size;
}} ImageEntry;

typedef struct ImageBundleHeader {{
    uint8_t  magic[4];
    uint32_t version;
    uint16_t count;
    uint16_t flags;
    uint32_t fontsOffset;  // 0 if no font section
}} ImageBundleHeader;

typedef struct FontSectionHeader {{
    uint8_t  magic[4];     // "LFNT"
    uint16_t pixelSize;
    int16_t  ascent;
    int16_t  descent;
    uint16_t lineHeight;
    uint16_t glyphCount;
    uint16_t reserved;
}} FontSectionHeader;

typedef struct GlyphEntry {{
    uint32_t codepoint;
    uint16_t width;        // actual cropped bitmap width
    uint16_t height;       // actual cropped bitmap height
    int16_t  bearingX;
    int16_t  bearingY;
    uint16_t advance;
    uint8_t  formatInfo;   // FMT_A8_RLE
    uint8_t  pad;
    uint32_t offset;       // absolute in bundle
    uint32_t size;
}} GlyphEntry;
#pragma pack(pop)

#ifdef __cplusplus
extern "C" {{
#endif
extern const uint8_t _binary_res_images_bin_start[];
extern const uint8_t _binary_res_images_bin_end[];
#ifdef __cplusplus
}}
#endif

#ifndef RES_IMAGE_BUNDLE
#define RES_IMAGE_BUNDLE  _binary_res_images_bin_start
#endif

#ifdef __cplusplus
static inline const ImageBundleHeader* resHeader() {{
    return (const ImageBundleHeader*)RES_IMAGE_BUNDLE;
}}
static inline const ImageEntry* imageEntry(ImageId id) {{
    return &((const ImageEntry*)(RES_IMAGE_BUNDLE + sizeof(ImageBundleHeader)))[id];
}}
static inline const void* imagePixels(ImageId id) {{
    return (const void*)(RES_IMAGE_BUNDLE + imageEntry(id)->offset);
}}
// Palette pointer for FMT_PAL_RLE / FMT_PAL_ALPHA_RLE.
// Palette is stored at the beginning of the data chunk, before the RLE stream.
static inline const uint16_t* imagePalette(ImageId id) {{
    int palBits = LITHO_PALETTE_BITS(imageEntry(id)->formatInfo);
    return (palBits > 0) ? (const uint16_t*)imagePixels(id) : nullptr;
}}
// Byte offset from pixel data start to the RLE row-offset table
// (skips the variable-size palette).
static inline uint32_t imageRleOffset(ImageId id) {{
    return (uint32_t)LITHO_PALETTE_SIZE(imageEntry(id)->formatInfo) * 2;
}}

static inline const FontSectionHeader* fontSection() {{
    uint32_t off = resHeader()->fontsOffset;
    if (!off || !(resHeader()->flags & RES_FLAG_HAS_FONTS)) return nullptr;
    return (const FontSectionHeader*)(RES_IMAGE_BUNDLE + off);
}}
static inline uint16_t fontGlyphCount() {{
    const FontSectionHeader* fh = fontSection();
    return fh ? fh->glyphCount : 0;
}}
static inline const GlyphEntry* fontGlyphAt(uint16_t index) {{
    const FontSectionHeader* fh = fontSection();
    if (!fh || index >= fh->glyphCount) return nullptr;
    return &((const GlyphEntry*)(RES_IMAGE_BUNDLE + resHeader()->fontsOffset
                                 + sizeof(FontSectionHeader)))[index];
}}
// Binary search: glyphs are packed in ascending unicode order.
static inline const GlyphEntry* fontFindGlyph(uint32_t codepoint) {{
    const FontSectionHeader* fh = fontSection();
    if (!fh || fh->glyphCount == 0) return nullptr;
    const GlyphEntry* tab = (const GlyphEntry*)(
        RES_IMAGE_BUNDLE + resHeader()->fontsOffset + sizeof(FontSectionHeader));
    int lo = 0, hi = (int)fh->glyphCount - 1;
    while (lo <= hi) {{
        int mid = (lo + hi) >> 1;
        uint32_t cp = tab[mid].codepoint;
        if (cp == codepoint) return &tab[mid];
        if (cp < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }}
    return nullptr;
}}
static inline const void* glyphPixels(const GlyphEntry* g) {{
    return g ? (const void*)(RES_IMAGE_BUNDLE + g->offset) : nullptr;
}}
#endif

// Q15 sin/cos — always available (not packed into the bundle).
#include "core/sin_table.hpp"
"""


def write_headers(gen_dir, bundle_name, version, entries):
    """Write res_images.h."""

    def img_id(e):
        return f"IMG_{e['prefix']}{safe_enum_name(e['name'])}"
    enum_lines = [f"    {img_id(e)} = {e['id']}," for e in entries]

    h_path = gen_dir / "res_images.h"
    with open(h_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(RES_H_TEMPLATE.format(
            bundle_name=bundle_name,
            version=version,
            enum_entries="\n".join(enum_lines),
            count=len(entries),
        ))


# ── main ──

def main():
    if len(sys.argv) < 2:
        print("Usage: pack_res.py <ui_dir> [output_dir]")
        sys.exit(1)

    ui_dir = Path(sys.argv[1]).resolve()
    if not ui_dir.is_dir():
        print(f"ERROR: {ui_dir} is not a directory")
        sys.exit(1)

    global GEN_DIR
    if len(sys.argv) > 2:
        GEN_DIR = Path(sys.argv[2]).resolve()
    else:
        GEN_DIR = ui_dir.parent.parent / "generated"
    GEN_DIR.mkdir(exist_ok=True)

    # Read info.txt
    info_path = ui_dir / "info.txt"
    bundle_name = ui_dir.name
    version = 1
    if info_path.exists():
        for line in info_path.read_text().strip().splitlines():
            line = line.strip()
            if '=' in line:
                k, v = line.split('=', 1)
                k, v = k.strip(), v.strip()
                if k == "name": bundle_name = v
                elif k == "version": version = int(v)

    print(f"UI: {bundle_name}  version={version}")

    # Directory → format mapping
    type_map = [
        # (dirname,     prefix,     pack_fn,         with_alpha)
        ("tint",        PRE_GRAY,   pack_grayscale,  False),
        ("solid",       PRE_OPAQUE, pack_pal8,       False),
        ("alpha",       PRE_ALPHA,  pack_pal8,       True),
    ]

    all_entries = []

    for dirname, prefix, pack_fn, with_alpha in type_map:
        sub = ui_dir / dirname
        if not sub.is_dir():
            continue
        pngs = sorted(sub.glob("*.png"))
        if not pngs:
            continue

        print(f"\n  [{dirname}/]  {len(pngs)} files  default_prefix='{prefix}'")
        for p in pngs:
            file_prefix = prefix
            name = p.stem

            if pack_fn is pack_grayscale:
                # Grayscale → always FMT_A8_RLE (RLE always beats raw for UI icons)
                w, h, gray = pack_fn(p)
                chunk = encode_rle(gray, w, h)
                actual_fmt = FMT_A8_RLE
                palette_bits = 0

            elif pack_fn is pack_pal8:
                # Palette-based: try PAL RLE vs RGB565 RLE, pick smallest
                w, h, pal565, pal_bits, idx, alpha_data = pack_pal8(p, with_alpha)
                # Pack palette at actual size (dynamic, not padded to 256)
                # Pad palette to declared power-of-2 size so decoder sees exactly palCount entries
                pal_count = 1 << pal_bits
                pal_bytes = struct.pack(f"<{pal_count}H", *(list(pal565) + [0] * (pal_count - len(pal565))))
                palette_bits = pal_bits

                if with_alpha and alpha_data and has_meaningful_alpha(alpha_data):
                    # Alpha image: compete PAL_ALPHA_RLE vs RGB565A_RLE
                    pal_rle_chunk = pal_bytes + encode_rle(idx, w, h, alpha=alpha_data)
                    pixels = [pal565[i] for i in idx]
                    rgb565a_rle = encode_rle_rgb565_alpha(pixels, alpha_data, w, h)
                    choices = [(len(pal_rle_chunk), FMT_PAL_ALPHA_RLE, pal_rle_chunk)]
                    if rgb565a_rle is not None:
                        choices.append((len(rgb565a_rle), FMT_RGB565A_RLE, rgb565a_rle))
                    best = min(choices, key=lambda x: x[0])
                    _, actual_fmt, chunk = best
                else:
                    # Opaque image: compete PAL_RLE vs RGB565_RLE
                    pal_rle_chunk = pal_bytes + encode_rle(idx, w, h)
                    pixels = [pal565[i] for i in idx]
                    rgb565_rle = rle_encode_rgb565(pixels, w, h)
                    choices = [(len(pal_rle_chunk), FMT_PAL_RLE, pal_rle_chunk)]
                    if rgb565_rle is not None:
                        choices.append((len(rgb565_rle), FMT_RGB565_RLE, rgb565_rle))
                    best = min(choices, key=lambda x: x[0])
                    _, actual_fmt, chunk = best

            entry = {
                "id":         len(all_entries),
                "name":       name,
                "prefix":     file_prefix,
                "width":      w,
                "height":     h,
                "formatInfo": make_format_info(actual_fmt, palette_bits),
                "fmt":        actual_fmt,   # for display only
                "palBits":    palette_bits,  # for display only
                "offset":     0,
                "size":       0,
                "chunk":      chunk,
            }
            all_entries.append(entry)
            print(f"    {file_prefix}{name:20s} {w}x{h}  fmt={actual_fmt} palBits={palette_bits}  {len(chunk)}B")

    # Optional charset-driven font section
    font_info = None
    fonts_dir = ui_dir / "fonts"
    if fonts_dir.is_dir():
        font_info = build_font_section(fonts_dir)

    # Write outputs
    bin_path = GEN_DIR / "res_images.bin"
    write_bin(bin_path, all_entries, font_info)
    bin_size = os.path.getsize(bin_path)
    ng = len(font_info["glyphs"]) if font_info else 0
    print(f"\n  Bundle: {bin_path.name} ({bin_size} bytes, "
          f"{len(all_entries)} images, {ng} glyphs)")

    write_headers(GEN_DIR, bundle_name, version, all_entries)
    print(f"  Headers: res_images.h")


if __name__ == "__main__":
    main()
