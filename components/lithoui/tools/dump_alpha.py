#!/usr/bin/env python3
"""dump_alpha.py — decode icons out of res_images.bin exactly like painter.hpp.

Replicates the firmware RLE decoders (all 5 formats, focus on the two alpha
formats) so we can (a) eyeball the rendered result and (b) detect structural
glitches — a mis-decoded RLE row shows up as a single bad horizontal line.

Usage:  python tools/dump_alpha.py [bin_path] [out_dir]
Default bin:  generated/res_images.bin   out_dir: dump/
"""

import os, struct, sys
from pathlib import Path
from PIL import Image

# ── format helpers (mirror res_images.h macros) ──
def LITHO_FORMAT(info):       return info & 0x07
def LITHO_PALETTE_BITS(info): return (info >> 3) & 0x1F
def LITHO_PALETTE_SIZE(info):
    b = LITHO_PALETTE_BITS(info)
    return (1 << b) if b else 0

kTTAlpha = [0, 85, 170, 213]          # painter.hpp:31

def rgb565_to_rgb888(c):
    r5 = (c >> 11) & 0x1F; g6 = (c >> 5) & 0x3F; b5 = c & 0x1F
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))

# ── per-image decode → (rgba pixels, list of row diagnostics) ──
class RowErr:
    __slots__ = ("y", "kind", "px")
    def __init__(self, y, kind, px): self.y, self.kind, self.px = y, kind, px

def decode(entry, chunk):
    w, h, info = entry["width"], entry["height"], entry["formatInfo"]
    fmt = LITHO_FORMAT(info)
    palN = LITHO_PALETTE_SIZE(info)
    # painter.hpp only skips a palette for fmt 1 (PAL) and 2 (PAL_ALPHA).
    # fmt 0/3/4 read the RLE stream straight from src, even though the packer
    # may leave a stray paletteBits in formatInfo — mirror that exactly.
    has_pal = fmt in (1, 2)
    palBytes = palN * 2 if has_pal else 0
    pal = [struct.unpack_from("<H", chunk, i * 2)[0] for i in range(palN)] if has_pal else []
    rle = chunk[palBytes:]
    off = [struct.unpack_from("<I", rle, y * 4)[0] for y in range(h)]

    rgba = bytearray(w * h * 4)   # RGBA8888, default fully transparent black
    errs = []

    def put(x, y, c565, a):
        r, g, b = rgb565_to_rgb888(c565)
        o = (y * w + x) * 4
        rgba[o] = r; rgba[o+1] = g; rgba[o+2] = b; rgba[o+3] = a

    for y in range(h):
        rowoff = off[y]
        raw = bool(rowoff & 0x80000000)
        base = rowoff & 0x7FFFFFFF
        p = base
        px = 0
        guard = 0
        try:
            if fmt in (2, 4):  # alpha formats — variable-length head, never raw
                while px < w:
                    guard += 1
                    if guard > w * 2 + 8:
                        errs.append(RowErr(y, "loop-overflow", px)); break
                    head = rle[p]; p += 1
                    if head & 0x80:                     # opaque, α=255, run 1..128
                        n = (head & 0x7F) + 1
                        if fmt == 2:
                            ix = rle[p]; p += 1; c = pal[ix] if ix < len(pal) else 0
                        else:
                            c = struct.unpack_from("<H", rle, p)[0]; p += 2
                        for k in range(n):
                            if px + k < w: put(px + k, y, c, 255)
                        px += n
                    else:                               # non-opaque, TT + run 1..32
                        tt = (head >> 5) & 0x03
                        n = (head & 0x1F) + 1
                        if tt == 0:                     # fully transparent, no color
                            px += n
                        else:
                            if fmt == 2:
                                ix = rle[p]; p += 1; c = pal[ix] if ix < len(pal) else 0
                            else:
                                c = struct.unpack_from("<H", rle, p)[0]; p += 2
                            a = kTTAlpha[tt]
                            for k in range(n):
                                if px + k < w: put(px + k, y, c, a)
                            px += n
            elif fmt == 0:  # A8_RLE grayscale, opaque
                if raw:
                    for x in range(w):
                        g = rle[base + x]; put(x, y, ((g>>3)<<11)|((g>>2)<<5)|(g>>3), 255)
                else:
                    while px < w:
                        g = rle[p]; ln = rle[p+1]; p += 2; n = ln + 1
                        for k in range(n):
                            if px + k < w: put(px+k, y, ((g>>3)<<11)|((g>>2)<<5)|(g>>3), 255)
                        px += n
            elif fmt == 1:  # PAL_RLE opaque
                if raw:
                    for x in range(w):
                        ix = rle[base + x]; put(x, y, pal[ix] if ix < len(pal) else 0, 255)
                else:
                    while px < w:
                        ix = rle[p]; ln = rle[p+1]; p += 2; n = ln + 1
                        c = pal[ix] if ix < len(pal) else 0
                        for k in range(n):
                            if px + k < w: put(px+k, y, c, 255)
                        px += n
            elif fmt == 3:  # RGB565_RLE opaque (run/literal)
                if raw:
                    for x in range(w):
                        c = struct.unpack_from("<H", rle, base + x*2)[0]; put(x, y, c, 255)
                else:
                    while px < w:
                        cmd = rle[p]; p += 1; n = (cmd & 0x7F) + 1
                        if cmd & 0x80:                  # literal
                            for k in range(n):
                                c = struct.unpack_from("<H", rle, p)[0]; p += 2
                                if px + k < w: put(px+k, y, c, 255)
                        else:                           # run
                            c = struct.unpack_from("<H", rle, p)[0]; p += 2
                            for k in range(n):
                                if px + k < w: put(px+k, y, c, 255)
                        px += n
        except (IndexError, struct.error):
            errs.append(RowErr(y, "buffer-overrun", px)); continue

        # raw-fallback rows write via the x-loop without advancing px — skip them
        if not raw and px != w:
            errs.append(RowErr(y, f"px={px}!={w}", px))

    return bytes(rgba), errs

# ── checkerboard composite so transparency + stray lines are visible ──
def on_checker(img, cell=8, c0=(90,90,90), c1=(150,150,150)):
    w, h = img.size
    bg = Image.new("RGB", (w, h))
    px = bg.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = c0 if ((x // cell) ^ (y // cell)) & 1 else c1
    bg.paste(img, (0, 0), img)
    return bg

def main():
    root = Path(__file__).resolve().parent.parent      # components/lithoui
    bin_path = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "generated" / "res_images.bin"
    out_dir  = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "dump"
    out_dir.mkdir(exist_ok=True)

    data = bin_path.read_bytes()
    magic, ver, count, flags, fontsOff = struct.unpack_from("<4sIHHI", data, 0)
    assert magic == b"LIMB", f"bad magic {magic!r}"
    print(f"bundle: {bin_path.name}  ver={ver:#x} count={count} flags={flags:#x} fontsOff={fontsOff}")

    # enum names from res_images.h (for filenames)
    names = {}
    hh = (root / "generated" / "res_images.h").read_text(errors="ignore")
    import re
    for m in re.finditer(r"IMG_(\w+)\s*=\s*(\d+)", hh):
        names[int(m.group(2))] = m.group(1)

    FMT = {0:"A8_RLE",1:"PAL_RLE",2:"PAL_ALPHA_RLE",3:"RGB565_RLE",4:"RGB565A_RLE"}
    entries = []
    for i in range(count):
        base = 16 + i*16
        eid, w, h, info, resv, off, size = struct.unpack_from("<HHHBBII", data, base)
        entries.append(dict(id=eid, width=w, height=h, formatInfo=info, offset=off, size=size))

    alpha_imgs, bad = [], []
    print(f"\n{'id':>3} {'name':22} {'WxH':>9} {'fmt':14} {'palB':>4} {'bytes':>7}  rows")
    for e in entries:
        nm = names.get(e["id"], f"img{e['id']}")
        chunk = data[e["offset"]: e["offset"] + e["size"]]
        rgba, errs = decode(e, chunk)
        fmt = LITHO_FORMAT(e["formatInfo"])
        flag = ""
        if errs:
            kinds = {}
            for er in errs: kinds[er.kind.split('=')[0].split('!')[0]] = kinds.get(er.kind.split('=')[0].split('!')[0],0)+1
            flag = "  <== " + ", ".join(f"{k}x{v}" for k,v in kinds.items())
            bad.append((nm, e, errs))
        print(f"{e['id']:>3} {nm:22} {e['width']:>4}x{e['height']:<4} {FMT.get(fmt,'?'):14} "
              f"{LITHO_PALETTE_BITS(e['formatInfo']):>4} {e['size']:>7}{flag}")

        img = Image.frombytes("RGBA", (e["width"], e["height"]), rgba)
        if fmt in (2, 4):
            alpha_imgs.append((nm, img))
            img.save(out_dir / f"{nm}.png")
            on_checker(img).save(out_dir / f"{nm}_checker.png")

    # montage of alpha icons on checkerboard
    if alpha_imgs:
        CELL = 120; cols = 6
        rows = (len(alpha_imgs) + cols - 1) // cols
        pad = 6; lab = 12
        cw, ch = CELL + pad, CELL + pad + lab
        sheet = Image.new("RGB", (cols*cw, rows*ch), (30,30,30))
        for i, (nm, img) in enumerate(alpha_imgs):
            cx, cy = (i % cols)*cw, (i // cols)*ch
            th = img.copy(); th.thumbnail((CELL, CELL))
            cell = on_checker(th)
            sheet.paste(cell, (cx + (CELL-cell.width)//2 + pad//2, cy + pad//2 + lab))
        sheet.save(out_dir / "_montage.png")
        print(f"\nmontage: {out_dir/'_montage.png'}  ({len(alpha_imgs)} alpha icons)")

    print(f"\n{'='*60}\nSUMMARY: {len(bad)} image(s) with row anomalies")
    for nm, e, errs in bad:
        ys = [er.y for er in errs]
        print(f"  {nm:22} fmt={LITHO_FORMAT(e['formatInfo'])} "
              f"{len(errs)} bad rows, y={ys[:12]}{'...' if len(ys)>12 else ''}")

if __name__ == "__main__":
    main()
