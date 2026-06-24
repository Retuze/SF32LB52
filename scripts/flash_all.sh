#!/bin/bash
# flash_all.sh — Flash bootloader + firmware in sequence
#
# Usage: bash scripts/flash_all.sh <COM_PORT> [--release]
#   e.g.  bash scripts/flash_all.sh COM3
#
# Requires: tools/sftool/sftool.exe (or in PATH)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SFTOOL="${ROOT}/tools/sftool/sftool.exe"
CHIP="SF32LB52"

PORT="${1:?Usage: $0 <COM_PORT> [--debug|--release]}"
PRESET="${2:-debug}"

FTAB="${ROOT}/tools/ftab.bin"
BOOTLOADER="${ROOT}/build/${PRESET}/bootloader.elf"
FIRMWARE="${ROOT}/build/${PRESET}/firmware.elf"

if [ ! -f "$SFTOOL" ]; then
    echo "sftool not found at $SFTOOL"
    exit 1
fi

echo "=== Flash ftab ==="
if [ -f "$FTAB" ]; then
    "$SFTOOL" -c "$CHIP" -p "$PORT" write_flash "${FTAB}@0x12000000"
else
    echo "ftab not found — generate with: python tools/gen_ftab.py tools/ftab.bin"
    exit 1
fi

echo "=== Flash bootloader ==="
if [ -f "$BOOTLOADER" ]; then
    "$SFTOOL" -c "$CHIP" -p "$PORT" write_flash "$BOOTLOADER"
else
    echo "Bootloader not built — skipping ($BOOTLOADER)"
fi

echo "=== Flash firmware ==="
if [ -f "$FIRMWARE" ]; then
    "$SFTOOL" -c "$CHIP" -p "$PORT" write_flash "$FIRMWARE"
else
    echo "Firmware not built — skipping ($FIRMWARE)"
    exit 1
fi

echo "=== Done ==="
