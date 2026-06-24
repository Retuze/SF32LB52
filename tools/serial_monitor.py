#!/usr/bin/env python3
"""serial_monitor.py — Read and display serial output from COM port.

Usage: python tools/serial_monitor.py COM26 [baud_rate]
Default baud: 1000000 (1M, SF32LB52 ROM bootloader default)
"""

import sys
import serial
import time


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM26"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 1000000

    print(f"[serial_monitor] Opening {port} at {baud} baud...")
    ser = serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.1,
    )

    print(f"[serial_monitor] Connected. Waiting for data... (Ctrl+C to exit)")
    print("-" * 60)

    buf = b""
    try:
        while True:
            data = ser.read(4096)
            if data:
                # Try to decode and print as text
                for b in data:
                    if 0x20 <= b <= 0x7E or b in (0x0A, 0x0D):
                        # Printable ASCII + CR/LF
                        sys.stdout.write(chr(b))
                        sys.stdout.flush()
                    elif b == 0x00:
                        pass  # skip nulls
                    else:
                        # Show hex for non-printable
                        sys.stdout.write(f"\\x{b:02X}")
                        sys.stdout.flush()
            time.sleep(0.01)
    except KeyboardInterrupt:
        print(f"\n[serial_monitor] Stopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
