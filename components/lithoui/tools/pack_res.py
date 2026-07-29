#!/usr/bin/env python3
"""pack_res.py — pack typed UI resources into res_images.bin + headers.

Usage:  python3 tools/pack_res.py <ui_dir> [output_dir]

Directory layout:
    ui/<name>/
      info.txt              name=..., version=...
      tint/                 grayscale -> FMT_A8_RLE (tintable at runtime)
      solid/                opaque color -> FMT_PAL_RLE or FMT_RGB565_RLE
      alpha/                color + alpha -> FMT_PAL_ALPHA_RLE or FMT_RGB565A_RLE
                            (files starting with r_ get R_ prefix → triggers sin table)

Outputs:
    res_images.bin    binary bundle
    res_images.h      ImageId enum, ImageEntry, inline accessors
"""

import os, struct, sys, math
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)

# ── globals (set in main) ──

GEN_DIR    = None
BUNDLE_NAME = None

# ── constants ──

MAGIC       = b"LIMB"
VERSION     = 0x00010000
ENTRY_SIZE  = 16
HEADER_SIZE = 16

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

FLAG_HAS_SIN  = 0x0001

# Directory prefixes
PRE_OPAQUE = ""
PRE_ALPHA  = "A_"
PRE_GRAY   = "G_"
PRE_ROT    = "R_"


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


# ── sin table ──

def generate_sin_table():
    """Q15 sin table [0°, 360°). Returns list of 360 int16_t."""
    return [int(round(math.sin(math.radians(d)) * 32767)) for d in range(360)]


# ── binary writer ──

def write_bin(path, entries, data_chunks, sin_table):
    """Write res_images.bin."""
    has_sin = sin_table is not None
    flags = FLAG_HAS_SIN if has_sin else 0

    off = HEADER_SIZE + len(entries) * ENTRY_SIZE
    for e in entries:
        e["offset"] = off
        e["size"] = len(e["chunk"])
        off += len(e["chunk"])

    sin_offset = off if has_sin else 0

    with open(path, "wb") as f:
        # Header
        f.write(MAGIC)
        f.write(struct.pack("<I", VERSION))
        f.write(struct.pack("<H", len(entries)))
        f.write(struct.pack("<H", flags))
        f.write(struct.pack("<I", sin_offset))

        # Entries
        for e in entries:
            f.write(struct.pack("<HHHBBII",
                     e["id"], e["width"], e["height"],
                     e["formatInfo"], 0,  # reserved byte
                     e["offset"], e["size"]))

        # Pixel data
        for e in entries:
            f.write(e["chunk"])

        # Sin table
        if has_sin:
            f.write(struct.pack(f"<{360}h", *sin_table))


# ── header writer ──

RES_H_TEMPLATE = """// Auto-generated by pack_res.py — DO NOT EDIT
#pragma once
#include <stdint.h>

#define RES_BUNDLE_NAME    "{bundle_name}"
#define RES_BUNDLE_VERSION 0x{version:08X}

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
    uint32_t sinOffset;
}} ImageBundleHeader;
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
    return &((const ImageEntry*)(RES_IMAGE_BUNDLE + 16))[id];
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
{sin_accessor}
#endif
"""


def write_headers(gen_dir, bundle_name, version, entries, sin_table):
    """Write res_images.h."""

    def img_id(e):
        return f"IMG_{e['prefix']}{safe_enum_name(e['name'])}"
    enum_lines = [f"    {img_id(e)} = {e['id']}," for e in entries]

    has_sin = sin_table is not None
    if has_sin:
        sin_acc = (
            "\n"
            "// Access the sin table embedded in the resource bundle.\n"
            "static inline const int16_t* resSinTable() {\n"
            "    return (const int16_t*)(RES_IMAGE_BUNDLE + resHeader()->sinOffset);\n"
            "}\n"
            "static inline int16_t sinDeg(int deg) {\n"
            "    const int16_t* t = resSinTable();\n"
            "    return t[(deg % 360 + 360) % 360];\n"
            "}\n"
            "static inline int16_t cosDeg(int deg) {\n"
            "    return sinDeg(deg + 90);\n"
            "}\n"
            "// Sub-degree sin/cos in deci-degrees (1/10 deg) via linear\n"
            "// interpolation of the per-degree table -- no extra memory,\n"
            "// enough resolution for a smooth sweeping hand.\n"
            "static inline int sinDeci(int dd) {\n"
            "    const int16_t* t = resSinTable();\n"
            "    dd = ((dd % 3600) + 3600) % 3600;\n"
            "    int d = dd / 10, f = dd % 10;\n"
            "    int s0 = t[d], s1 = t[(d + 1) % 360];\n"
            "    return s0 + (s1 - s0) * f / 10;\n"
            "}\n"
            "static inline int cosDeci(int dd) { return sinDeci(dd + 900); }\n"
        )
    else:
        sin_acc = (
            "\n"
            "static inline const int16_t* resSinTable() { return 0; }\n"
            "static inline int sinDeci(int dd) { (void)dd; return 0; }\n"
            "static inline int cosDeci(int dd) { (void)dd; return 0; }\n"
        )

    h_path = gen_dir / "res_images.h"
    with open(h_path, "w") as f:
        f.write(RES_H_TEMPLATE.format(
            bundle_name=bundle_name,
            version=version,
            enum_entries="\n".join(enum_lines),
            count=len(entries),
            sin_accessor=sin_acc,
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
            # In alpha/, files starting with r_ get R_ prefix (triggers sin table)
            file_prefix = prefix
            fname = p.stem
            if dirname == "alpha" and fname.lower().startswith("r_"):
                file_prefix = PRE_ROT
                fname = fname[2:]  # strip "r_" from name
            name = fname

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

    # Sin table — triggered by R_ prefix images (alpha/ files named r_*.png)
    rot_count = sum(1 for e in all_entries if e["prefix"] == PRE_ROT)
    sin_table = generate_sin_table() if rot_count > 0 else None
    if sin_table:
        print(f"\n  Sin table: 360 entries, {360*2} bytes (rotatable images: {rot_count})")

    # Write outputs
    bin_path = GEN_DIR / "res_images.bin"
    write_bin(bin_path, all_entries, [e["chunk"] for e in all_entries], sin_table)
    bin_size = os.path.getsize(bin_path)
    print(f"\n  Bundle: {bin_path.name} ({bin_size} bytes, {len(all_entries)} images)")

    write_headers(GEN_DIR, bundle_name, version, all_entries, sin_table)
    print(f"  Headers: res_images.h", end="")
    if sin_table:
        print(f" + sin_table.h")
    else:
        print()


if __name__ == "__main__":
    main()
