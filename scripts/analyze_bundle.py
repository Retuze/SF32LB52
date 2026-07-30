#!/usr/bin/env python3
"""Analyze res_images.bin — palette sizes, run length distributions, format stats."""

import struct, sys, os
from collections import Counter
from pathlib import Path

BUNDLE_PATH = Path(__file__).parent.parent / "components/lithoui/generated/res_images.bin"

FMT_NAMES = {
    0: "A8_RLE",
    1: "PAL_RLE",
    2: "PAL_ALPHA_RLE",
    3: "RGB565_RLE",
    4: "RGB565A_RLE",
}

def read_bundle(path):
    with open(path, "rb") as f:
        data = f.read()

    magic, ver, count, flags, fontsOff = struct.unpack_from("<4sIHHI", data, 0)
    assert magic == b"LIMB", f"Bad magic: {magic}"

    entries = []
    off = 16
    for i in range(count):
        eid, w, h, fmtInfo, reserved, eoff, esize = struct.unpack_from("<HHHBBII", data, off)
        entries.append({
            "id": eid, "width": w, "height": h,
            "format": fmtInfo & 0x07,
            "palBits": (fmtInfo >> 3) & 0x1F,
            "palCount": (1 << ((fmtInfo >> 3) & 0x1F)) if ((fmtInfo >> 3) & 0x1F) else 0,
            "offset": eoff, "size": esize,
        })
        off += 16

    return entries, data


def analyze_rle_a8(data_chunk, w, h):
    """A8_RLE: [gray][len], len = count-1 (1..256). Offset table at start of chunk."""
    off_table = struct.unpack_from(f"<{h}I", data_chunk, 0)
    runs = []
    for y in range(h):
        p = off_table[y]  # absolute offset from chunk start
        x = 0
        while x < w:
            l = data_chunk[p + 1] + 1
            runs.append(l)
            x += l
            p += 2
    return runs


def analyze_rle_pal(data_chunk, pal_count, w, h):
    """PAL_RLE: palette + [h*4 offset table][RLE stream]."""
    pal_bytes = pal_count * 2
    rle_start = pal_bytes  # offset table starts right after palette
    off_table = struct.unpack_from(f"<{h}I", data_chunk, rle_start)
    runs = []
    for y in range(h):
        p = rle_start + off_table[y]  # off_table[y] is relative to rle_start
        x = 0
        while x < w:
            l = data_chunk[p + 1] + 1
            runs.append(l)
            x += l
            p += 2
    return runs


def analyze_rle_pal_alpha(data_chunk, pal_count, w, h):
    """PAL_ALPHA_RLE: head byte [TT|LLLLLL]. TT=0→transp(1B), TT=1→opaque(2B), TT=2/3→alpha(2B)."""
    pal_bytes = pal_count * 2
    rle_start = pal_bytes
    off_table = struct.unpack_from(f"<{h}I", data_chunk, rle_start)
    transp_runs = []
    opaque_runs = []
    alpha_runs  = []
    for y in range(h):
        p = rle_start + off_table[y]
        x = 0
        while x < w:
            head = data_chunk[p]; p += 1
            tt = head >> 6
            n = (head & 0x3F) + 1
            if tt == 0:
                transp_runs.append(n)
            elif tt == 1:
                opaque_runs.append(n)
                p += 1  # palette idx
            else:
                alpha_runs.append(n)
                p += 1  # palette idx
            x += n
    return transp_runs, opaque_runs, alpha_runs


def analyze_rle_rgb565(data_chunk, w, h):
    """RGB565_RLE: [h*4 offset table][RLE stream]."""
    off_table = struct.unpack_from(f"<{h}I", data_chunk, 0)
    runs_list = []
    lit_list = []
    for y in range(h):
        p = off_table[y]  # absolute
        x = 0
        while x < w:
            cmd = data_chunk[p]
            n = (cmd & 0x7F) + 1
            if cmd & 0x80:
                lit_list.append(n)
                p += 1 + n * 2
            else:
                runs_list.append(n)
                p += 3  # cmd + color_lo + color_hi
            x += n
    return runs_list, lit_list


def analyze_rle_rgb565_alpha(data_chunk, w, h):
    """RGB565A_RLE: head byte [TT|LLLLLL]. TT=0→transp(1B), TT=1→opaque(3B), TT=2/3→alpha(3B)."""
    off_table = struct.unpack_from(f"<{h}I", data_chunk, 0)
    transp_runs = []
    opaque_runs = []
    alpha_runs  = []
    for y in range(h):
        p = off_table[y]
        x = 0
        while x < w:
            head = data_chunk[p]; p += 1
            tt = head >> 6
            n = (head & 0x3F) + 1
            if tt == 0:
                transp_runs.append(n)
            elif tt == 1:
                opaque_runs.append(n)
                p += 2  # color
            else:
                alpha_runs.append(n)
                p += 2  # color
            x += n
    return transp_runs, opaque_runs, alpha_runs


def histogram(runs, buckets=None):
    if not runs:
        return {}
    if buckets is None:
        buckets = [1, 2, 4, 8, 16, 32, 64, 128, 256]
    c = Counter()
    for r in runs:
        for b in buckets:
            if r <= b:
                c[b] += 1
                break
        else:
            c["256+"] = c.get("256+", 0) + 1
    total = len(runs)
    return {k: (v, v * 100.0 / total) for k, v in sorted(c.items(), key=lambda x: (isinstance(x[0], str), x[0]))}


def main():
    entries, data = read_bundle(BUNDLE_PATH)

    # Separate by size
    icons_100 = [e for e in entries if e["width"] == 100 and e["height"] == 100]
    large_360 = [e for e in entries if e["width"] == 360 and e["height"] == 360]
    others    = [e for e in entries if e not in icons_100 and e not in large_360]

    # ── 1. Palette color count distribution for 100x100 icons ──
    print("=" * 70)
    print("1. 100x100 图标调色板颜色数分布")
    print("=" * 70)
    pal_icons = [e for e in icons_100 if e["palCount"] > 0]
    by_pal = {}
    for e in pal_icons:
        k = e["palCount"]
        by_pal.setdefault(k, []).append(e)

    print(f"\n  {'Palette大小':<14} {'颜色数':<8} {'图片数':<8} {'调色板字节'}")
    print(f"  {'-'*50}")
    for pc in sorted(by_pal.keys()):
        imgs = by_pal[pc]
        pal_bytes = pc * 2
        names = ", ".join(f"{e['id']}" for e in imgs[:5])
        if len(imgs) > 5: names += f" ... ({len(imgs)} total)"
        print(f"  {pc:>4} 项 (2^{e['palBits']})   {pc:>4}色    {len(imgs):>4} 张    {pal_bytes}B")
    print(f"\n  动态调色板总计: {sum(e['palCount']*2 for e in pal_icons)}B (旧固定 256 色 = {len(pal_icons)*512}B, 省 {len(pal_icons)*512 - sum(e['palCount']*2 for e in pal_icons)}B)")

    # ── 2. Format distribution ──
    print("\n" + "=" * 70)
    print("2. 格式分布")
    print("=" * 70)

    def fmt_stats(group, label):
        print(f"\n  [{label}]  {len(group)} 张图:")
        by_fmt = {}
        for e in group:
            by_fmt.setdefault(e["format"], []).append(e)
        for fmt in sorted(by_fmt.keys()):
            imgs = by_fmt[fmt]
            total_bytes = sum(e["size"] for e in imgs)
            print(f"    {FMT_NAMES.get(fmt, '?'):20s}: {len(imgs):>3} 张  {total_bytes:>8}B")

    fmt_stats(icons_100, "100×100 icons")
    fmt_stats(large_360, "360×360 large")
    fmt_stats(others, "other sizes")

    # ── 3. Run length distribution for 100x100 icons ──
    print("\n" + "=" * 70)
    print("3. 100×100 图标 RLE run 长度分布")
    print("=" * 70)

    for fmt in sorted(set(e["format"] for e in icons_100)):
        fmt_imgs = [e for e in icons_100 if e["format"] == fmt]
        print(f"\n  [{FMT_NAMES.get(fmt, '?')}]  {len(fmt_imgs)} 张图:")

        all_runs = {}
        for e in fmt_imgs:
            chunk = data[e["offset"]:e["offset"]+e["size"]]
            w, h = e["width"], e["height"]

            if fmt == 0:
                runs = analyze_rle_a8(chunk, w, h)
                all_runs.setdefault("runs", []).extend(runs)
            elif fmt == 1:
                runs = analyze_rle_pal(chunk, e["palCount"], w, h)
                all_runs.setdefault("opaque", []).extend(runs)
            elif fmt == 2:
                tr, op, al = analyze_rle_pal_alpha(chunk, e["palCount"], w, h)
                all_runs.setdefault("opaque", []).extend(op)
                all_runs.setdefault("alpha", []).extend(al)
                all_runs.setdefault("transp", []).extend(tr)
            elif fmt == 3:
                runs, lits = analyze_rle_rgb565(chunk, w, h)
                all_runs.setdefault("runs", []).extend(runs)
                all_runs.setdefault("literals", []).extend(lits)
            elif fmt == 4:
                tr, op, al = analyze_rle_rgb565_alpha(chunk, w, h)
                all_runs.setdefault("opaque", []).extend(op)
                all_runs.setdefault("alpha", []).extend(al)
                all_runs.setdefault("transp", []).extend(tr)

        for cat, runs in all_runs.items():
            if not runs:
                continue
            avg = sum(runs) / len(runs)
            med = sorted(runs)[len(runs)//2]
            print(f"    {cat:12s}: total={len(runs):>6}  avg={avg:5.1f}  median={med:>4}  max={max(runs):>4}")
            hist = histogram(runs, [1, 2, 4, 8, 16, 32, 64, 128, 256])
            print(f"              分布: ", end="")
            parts = []
            for b, (cnt, pct) in hist.items():
                if pct > 0.5:
                    parts.append(f"≤{b}:{pct:.0f}%")
            print(", ".join(parts))

    # ── 4. Run length for 360×360 ──
    print("\n" + "=" * 70)
    print("4. 360×360 大图 RLE run 长度分布")
    print("=" * 70)

    for e in large_360:
        chunk = data[e["offset"]:e["offset"]+e["size"]]
        w, h = e["width"], e["height"]
        fmt = e["format"]

        print(f"\n  ID={e['id']:>2}  {FMT_NAMES.get(fmt, '?')}  palBits={e['palBits']}  {w}×{h}  {e['size']}B")

        if fmt == 3:
            runs, lits = analyze_rle_rgb565(chunk, w, h)
            if runs:
                avg = sum(runs) / len(runs)
                med = sorted(runs)[len(runs)//2]
                print(f"    runs:     total={len(runs):>6}  avg={avg:5.1f}  median={med:>4}  max={max(runs):>4}")
                hist = histogram(runs, [1, 2, 4, 8, 16, 32, 64, 128, 256])
                print(f"             分布: ", end="")
                parts = []
                for b, (cnt, pct) in hist.items():
                    if pct > 0.5:
                        parts.append(f"≤{b}:{pct:.0f}%")
                print(", ".join(parts))
            if lits:
                avg = sum(lits) / len(lits)
                print(f"    literals: total={len(lits):>6}  avg={avg:5.1f}  median={sorted(lits)[len(lits)//2]:>4}  max={max(lits):>4}")
                hist = histogram(lits, [1, 2, 4, 8, 16, 32, 64, 128, 256])
                print(f"             分布: ", end="")
                parts = []
                for b, (cnt, pct) in hist.items():
                    if pct > 0.5:
                        parts.append(f"≤{b}:{pct:.0f}%")
                print(", ".join(parts))

        elif fmt == 2:
            tr, op, al = analyze_rle_pal_alpha(chunk, e["palCount"], w, h)
            for cat, runs in [("opaque", op), ("alpha", al), ("transp", tr)]:
                if not runs:
                    continue
                avg = sum(runs) / len(runs)
                med = sorted(runs)[len(runs)//2]
                print(f"    {cat:8s}: total={len(runs):>6}  avg={avg:5.1f}  median={med:>4}  max={max(runs):>4}")
                hist = histogram(runs, [1, 2, 4, 8, 16, 32, 64, 128, 256])
                print(f"             分布: ", end="")
                parts = []
                for b, (cnt, pct) in hist.items():
                    if pct > 0.5:
                        parts.append(f"≤{b}:{pct:.0f}%")
                print(", ".join(parts))

    # ── 5. Compression summary ──
    print("\n" + "=" * 70)
    print("5. 压缩效率总览")
    print("=" * 70)
    for label, group in [("100×100 icons", icons_100), ("360×360 large", large_360)]:
        total_raw = sum(e["width"] * e["height"] * 2 for e in group)
        total_rle = sum(e["size"] for e in group)
        ratio = total_rle * 100.0 / total_raw
        print(f"\n  {label}: raw={total_raw}B  RLE={total_rle}B  ({ratio:.1f}% of raw)")

    # Palette savings
    total_old_pal = sum(512 for e in entries if e["palCount"] > 0)
    total_new_pal = sum(e["palCount"] * 2 for e in entries if e["palCount"] > 0)
    print(f"\n  调色板总字节: {total_new_pal}B (旧固定={total_old_pal}B, 省 {total_old_pal - total_new_pal}B = {(total_old_pal-total_new_pal)*100/total_old_pal:.0f}%)")


if __name__ == "__main__":
    main()
