#!/usr/bin/env python3
"""gen_ftab.py — Generate minimal flash table (sec_config) binary for SF32LB52.

Layout (matching ptab.json from SDK):
  ftab        @ 0x12000000  32 KB  (flash table + calibration)
  bootloader  @ 0x12010000  64 KB  (ROM copies to RAM 0x20020000, executes there)
  firmware    @ 0x12020000  ~14 MB (XIP from flash)

The ROM bootloader reads sec_config at 0x12000000, finds bootloader
entry at ftab[3], copies it to 0x20020000, and jumps to it.
"""

import struct
import sys

# ---- Constants from SDK dfu.h ----
SEC_CONFIG_MAGIC  = 0x53454346   # "SECF"
DFU_FLASH_PARTITION = 16
DFU_FLASH_IMG_IDX_MAX = DFU_FLASH_PARTITION - 2
CORE_LCPU = 0
CORE_BL   = 1
CORE_HCPU = 2
CORE_BOOT = 3
CORE_MAX = 4
DFU_SIG_KEY_SIZE = 256
DFU_FLAG_AUTO = 2

# ---- Flash table entry (16 bytes each) ----
# struct flash_table { uint32_t base; uint32_t size; uint32_t xip_base; uint32_t flags; }

def ftab_entry(base, size, xip_base=0, flags=0):
    return struct.pack('<IIII', base, size, xip_base, flags)

# ---- Image header (512 bytes for struct image_header_enc on flash) ----
# Useful fields are at the start: uint32_t length; uint16_t blksize; uint16_t flags.

def img_entry(length, blksize=512, flags=0):
    data = bytearray(512)
    struct.pack_into('<IHH', data, 0, length, blksize, flags)
    return bytes(data)

def build_sec_config():
    buf = bytearray()

    # 0x000: magic (4 bytes)
    buf += struct.pack('<I', SEC_CONFIG_MAGIC)

    # 0x004: ftab[0..15] (16 entries × 16 bytes = 256 bytes)
    # ftab[0]:  flash table itself
    buf += ftab_entry(0x12000000, 0x00008000, 0, 0)
    # ftab[1]:  calibration table (after ftab, before bootloader)
    buf += ftab_entry(0x12008000, 0x00008000, 0, 0)
    # ftab[2]:  unused
    buf += ftab_entry(0, 0, 0, 0)
    # ftab[3]:  bootloader (flash→RAM copy, exec at 0x20020000)
    buf += ftab_entry(0x12010000, 0x00010000, 0x20020000, 0)
    # ftab[4]:  main firmware (XIP from flash)
    buf += ftab_entry(0x12020000, 0x00700000, 0x12020000, 0)
    # ftab[5]:  bootloader patch (unused for now)
    buf += ftab_entry(0, 0, 0, 0)
    # ftab[6]:  unused
    buf += ftab_entry(0, 0, 0, 0)
    # ftab[7]:  bootloader (backup, flash→RAM copy)
    buf += ftab_entry(0x12010000, 0x00010000, 0x20020000, 0)
    # ftab[8]:  firmware (backup, XIP)
    buf += ftab_entry(0x12020000, 0x00700000, 0x12020000, 0)
    # ftab[9..15]: unused
    for i in range(9, 16):
        buf += ftab_entry(0, 0, 0, 0)

    # 0x104: sig_pub_key (256 bytes, zeroed = no secure boot)
    buf += b'\x00' * 256

    # 0x204: reserved padding to 4096
    pad = 4096 - len(buf)
    if pad > 0:
        buf += b'\x00' * pad

    # 0x1000: imgs[0..13] (14 image headers × 512 bytes)
    # imgs index = flash_id - DFU_FLASH_IMG_LCPU(2)
    # img[0]: LCPU (unused)
    buf += img_entry(0xFFFFFFFF)
    # img[1]: bootloader
    buf += img_entry(0x00010000, 512, DFU_FLAG_AUTO)
    # img[2]: HCPU firmware
    buf += img_entry(0x00700000, 512, DFU_FLAG_AUTO)
    # img[3]: boot (unused)
    buf += img_entry(0xFFFFFFFF)
    # img[4..5]: LCPU2/BCPU2 unused
    buf += img_entry(0xFFFFFFFF)
    buf += img_entry(0xFFFFFFFF)
    # img[6]: HCPU2 backup firmware
    buf += img_entry(0x00700000, 512, DFU_FLAG_AUTO)
    # img[7..13]: unused
    for i in range(7, DFU_FLASH_IMG_IDX_MAX):
        buf += img_entry(0xFFFFFFFF)

    # running_imgs[CORE_MAX] — pointers to image headers.
    # SDK core IDs: LCPU=0, BL=1, HCPU=2, BOOT=3.
    img_base = 0x12000000 + 4096
    UNUSED = 0xFFFFFFFF
    running = [UNUSED] * CORE_MAX
    running[CORE_BL] = img_base + 1 * 512
    running[CORE_HCPU] = img_base + 2 * 512
    for ptr in running:
        buf += struct.pack('<I', ptr)

    return bytes(buf)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "ftab.bin"
    data = build_sec_config()
    with open(out, 'wb') as f:
        f.write(data)
    print(f"[gen_ftab] Wrote {len(data)} bytes to {out}")
    print(f"  sec_config: {len(data)} bytes")
    print(f"  ftab[0]: flash_table @ 0x12000000")
    print(f"  ftab[3]: bootloader  @ 0x12010000 -> RAM 0x20020000")
    print(f"  ftab[4]: firmware    @ 0x12020000 (XIP)")


if __name__ == '__main__':
    main()
