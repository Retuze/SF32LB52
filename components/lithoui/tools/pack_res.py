#!/usr/bin/env python3
"""pack_res.py — pack typed UI resources into res_images.bin + headers.

Usage:  python3 tools/pack_res.py <ui_dir> [output_dir]

Directory layout:
    ui/<name>/
      info.txt              name=..., version=...
      opaque/               opaque color -> FMT_PAL8_RLE
      alpha/                color + alpha -> FMT_PAL8_RLE_ALPHA
      gray/                 grayscale -> FMT_A8_RLE (tintable)
      rotate/               rotation-ready -> FMT_PAL8_RLE_ALPHA (triggers sin table)

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

# Format enum — 7 formats
FMT_A8              = 0   # grayscale raw
FMT_A8_RLE          = 1   # grayscale + RLE
FMT_PAL8            = 2   # 256-color palette + raw index
FMT_PAL8_RLE        = 3   # 256-color palette + RLE
FMT_PAL8_ALPHA      = 4   # 256-color palette + raw index + raw alpha
FMT_PAL8_ALPHA_RLE  = 5   # 256-color palette + RLE with inline alpha
FMT_RGB565_RLE      = 6   # raw RGB565 + RLE (no palette, direct color)

# Alpha levels (2 bits → 4 levels) for alpha formats
ALPHA_LEVELS = [0, 85, 170, 255]

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


def quantize_alpha(a):
    """Map 0..255 alpha to nearest 2-bit level (0..3)."""
    if a >= 213: return 3       # 255
    if a >= 128: return 2       # 170
    if a >= 43:  return 1       # 85
    return 0                     # 0


# ── unified RLE encoder ─────────────────────────────────────────────

def encode_rle(values, w, h, alpha=None):
    """
    Encode a row-major array of bytes into RLE format.

    No-alpha formats (alpha=None, FMT_A8_RLE / FMT_PAL8_RLE):
      [value_byte][length_byte]  — length = count-1 (0..255, 1..256 pixels)

    Alpha format (FMT_PAL8_RLE_ALPHA):
      [value_byte][length_byte]
        length.bit7 = 0 → opaque, bits6:0 = count-1 (1..128)
        length.bit7 = 1 → alpha,  bits5:4 = alpha_level (0..3),
                          bits2:0 = count-1 (1..8)

    Returns bytes: [h*4 offset table][RLE stream].
    """
    out = bytearray(h * 4)  # placeholder for offset table
    max_run_no_alpha = 256   # full 8-bit length
    max_run_opaque   = 128   # bit7=0, bits6:0=7 bits

    for y in range(h):
        # Fill offset for this row
        struct.pack_into('<I', out, y * 4, len(out))

        row = values[y * w:(y + 1) * w]
        row_alpha = alpha[y * w:(y + 1) * w] if alpha else None

        x = 0
        while x < w:
            v = row[x]

            if row_alpha:
                # ── PAL8_RLE_ALPHA ──────────────────────────
                a_cur = quantize_alpha(row_alpha[x])

                if a_cur == 3:
                    # Opaque → use opaque encoding (bit7=0), up to 128
                    run = 1
                    while (x + run < w and run < max_run_opaque and
                           row[x + run] == v and quantize_alpha(row_alpha[x + run]) == 3):
                        run += 1
                    out.append(v)
                    out.append(run - 1)  # bit7=0, count-1
                    x += run
                else:
                    # Transparent → alpha encoding (bit7=1), up to 8
                    run = 1
                    while (x + run < w and run < 8 and
                           row[x + run] == v and quantize_alpha(row_alpha[x + run]) == a_cur):
                        run += 1
                    out.append(v)
                    out.append(0x80 | (a_cur << 4) | (run - 1))
                    x += run
            else:
                # ── A8_RLE / PAL8_RLE (no alpha) ─────────────
                run = 1
                while x + run < w and run < max_run_no_alpha and row[x + run] == v:
                    run += 1
                out.append(v)
                out.append(run - 1)  # full 8-bit: 0..255
                x += run

    return bytes(out)


def rle_encode_rgb565(pixels, w, h):
    """
    RLE compress RGB565 uint16 pixels with row offset table.
    cmd byte: bit7=0→run(count-1)[color×2]; bit7=1→literal(count-1)[pixels×2n]
    Returns bytes: [h*4 off][RLE stream], or None if RLE > raw.
    """
    off_size = h * 4
    out = bytearray(off_size)
    for y in range(h):
        struct.pack_into('<I', out, y * 4, len(out))
        row = pixels[y * w:(y + 1) * w]
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
    return bytes(out) if len(out) < w * h * 2 else None


# ── pack functions ──

def pack_grayscale(path):
    """PNG → grayscale values (0..255)."""
    img = Image.open(path).convert("RGB")
    w, h = img.size
    data = img.getdata()
    gray = [luminance(r, g, b) for r, g, b in data]
    return w, h, gray


def pack_pal8(path, with_alpha=False):
    """
    PNG → 256-color palette + index array + optional alpha array.

    Returns (w, h, pal565, index, alpha_or_None).
    """
    if with_alpha:
        img = Image.open(path).convert("RGBA")
        w, h = img.size
        data = list(img.getdata())
        # Separate RGB and alpha
        rgb_data = [(r, g, b) for r, g, b, a in data]
        alpha_data = [a for r, g, b, a in data]
    else:
        img = Image.open(path).convert("RGB")
        w, h = img.size
        rgb_data = list(img.getdata())
        alpha_data = None

    # Quantize RGB to 256 colors
    # Use Pillow's quantize on a temporary RGB image
    tmp = Image.new("RGB", (w, h))
    tmp.putdata(rgb_data)
    q = tmp.quantize(colors=256, method=Image.MEDIANCUT)
    idx = list(q.getdata())
    pal = q.getpalette() or []
    ncol = min(256, len(pal) // 3)
    pal565 = [rgba_to_rgb565(pal[i*3], pal[i*3+1], pal[i*3+2]) for i in range(ncol)]

    return w, h, pal565, idx, alpha_data


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
            f.write(struct.pack("<HHHHII",
                     e["id"], e["width"], e["height"],
                     e["fmt"], e["offset"], e["size"]))

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
    FMT_A8              = 0,  // grayscale raw
    FMT_A8_RLE          = 1,  // grayscale + RLE
    FMT_PAL8            = 2,  // 256-color palette + raw index
    FMT_PAL8_RLE        = 3,  // 256-color palette + RLE
    FMT_PAL8_ALPHA      = 4,  // 256-color palette + raw index + raw alpha
    FMT_PAL8_ALPHA_RLE  = 5,  // 256-color palette + RLE with inline alpha
    FMT_RGB565_RLE      = 6,  // raw RGB565 + RLE (direct color, no palette)
}};

#pragma pack(push, 1)
typedef struct ImageEntry {{
    uint16_t id;
    uint16_t width;
    uint16_t height;
    uint16_t format;
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

#define RES_IMAGE_BUNDLE  _binary_res_images_bin_start

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
        ("gray",        PRE_GRAY,   pack_grayscale,  False),
        ("opaque",      PRE_OPAQUE, pack_pal8,       False),
        ("alpha",       PRE_ALPHA,  pack_pal8,       True),
        ("rotate",      PRE_ROT,    pack_pal8,       True),
    ]

    all_entries = []

    for dirname, prefix, pack_fn, with_alpha in type_map:
        sub = ui_dir / dirname
        if not sub.is_dir():
            continue
        pngs = sorted(sub.glob("*.png"))
        if not pngs:
            continue

        print(f"\n  [{dirname}/]  {len(pngs)} files  prefix='{prefix}'")
        for p in pngs:
            name = p.stem

            if pack_fn is pack_grayscale:
                # Grayscale → A8_RLE or A8 (raw fallback if RLE > raw)
                w, h, gray = pack_fn(p)
                raw_bytes = struct.pack(f"<{w*h}B", *gray)
                rle = encode_rle(gray, w, h)
                if len(rle) < len(raw_bytes):
                    chunk = rle
                    actual_fmt = FMT_A8_RLE
                else:
                    chunk = raw_bytes
                    actual_fmt = FMT_A8

            elif pack_fn is pack_pal8:
                # Palette-based: try raw / RLE, pick smallest
                w, h, pal565, idx, alpha_data = pack_pal8(p, with_alpha)
                # Pack palette as uint16_t (compact), uint32_t word-fill done at decode time
                pal_bytes = struct.pack("<256H", *(list(pal565) + [0] * (256 - len(pal565))))
                raw_idx = struct.pack(f"<{w*h}B", *idx)

                if with_alpha and alpha_data and has_meaningful_alpha(alpha_data):
                    raw_alpha = struct.pack(f"<{w*h}B", *alpha_data)
                    raw_chunk = pal_bytes + raw_idx + raw_alpha
                    rle_chunk = pal_bytes + encode_rle(idx, w, h, alpha=alpha_data)
                    if len(rle_chunk) < len(raw_chunk):
                        chunk, actual_fmt = rle_chunk, FMT_PAL8_ALPHA_RLE
                    else:
                        chunk, actual_fmt = raw_chunk, FMT_PAL8_ALPHA
                else:
                    # Opaque: try PAL8 raw, PAL8_RLE, RGB565_RLE
                    raw_chunk = pal_bytes + raw_idx
                    rle_chunk = pal_bytes + encode_rle(idx, w, h)
                    # Also try RGB565_RLE (direct color, no palette)
                    pixels = [pal565[i] for i in idx]  # map index→RGB565
                    rgb565_rle = rle_encode_rgb565(pixels, w, h)
                    choices = [(len(raw_chunk), FMT_PAL8, raw_chunk)]
                    choices.append((len(rle_chunk), FMT_PAL8_RLE, rle_chunk))
                    if rgb565_rle is not None:
                        choices.append((len(rgb565_rle), FMT_RGB565_RLE, rgb565_rle))
                    best = min(choices, key=lambda x: x[0])
                    _, actual_fmt, chunk = best

            entry = {
                "id":     len(all_entries),
                "name":   name,
                "prefix": prefix,
                "width":  w,
                "height": h,
                "fmt":    actual_fmt,
                "offset": 0,
                "size":   0,
                "chunk":  chunk,
            }
            all_entries.append(entry)
            print(f"    {prefix}{name:20s} {w}x{h}  fmt={actual_fmt}  {len(chunk)}B")

    # Sin table — only if rotatable/ has images
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
