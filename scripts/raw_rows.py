import struct
from pathlib import Path

data = Path('E:/SF32/SF32LB52/components/lithoui/generated/res_images.bin').read_bytes()
_, _, count, _, _ = struct.unpack_from('<4sIHHI', data, 0)
FMT_NAMES = {0:'A8_RLE',1:'PAL_RLE',2:'PAL_ALPHA_RLE',3:'RGB565_RLE',4:'RGB565A_RLE'}

print(f'{"Name":20s} {"Fmt":14s} {"Size":>6s}  {"RLE":>5s}  {"Raw":>5s}  {"Raw%":>5s}')
print('-' * 65)

total_rle_rows = total_raw_rows = 0

for i in range(count):
    eid,w,h,fmtInfo,_,eoff,esize = struct.unpack_from('<HHHBBII', data, 16 + i*16)
    fmt = fmtInfo & 0x07
    palBits = (fmtInfo >> 3) & 0x1F
    pal_bytes = (1 << palBits) * 2 if palBits else 0
    chunk = data[eoff:eoff+esize]

    rle_rows = raw_rows = 0
    if fmt in (0, 1, 3):
        if fmt == 0:
            off_start = 0
        elif fmt == 1:
            off_start = pal_bytes
        else:
            off_start = 0
        off_table = struct.unpack_from(f'<{h}I', chunk, off_start)
        for o in off_table:
            if o & 0x80000000:
                raw_rows += 1
            else:
                rle_rows += 1
        total_rle_rows += rle_rows
        total_raw_rows += raw_rows

    if w == 100 and h == 100:
        total = rle_rows + raw_rows
        pct = raw_rows * 100.0 / total if total > 0 else 0
        name = f'id={eid}'
        if pct > 0:
            print(f'{name:20s} {FMT_NAMES.get(fmt,"?"):14s} {esize:>6}B  {rle_rows:>5}  {raw_rows:>5}  {pct:>4.0f}%')

print(f'\nTotal: {total_rle_rows} RLE rows + {total_raw_rows} raw rows')
print(f'Raw rows: {total_raw_rows*100.0/(total_rle_rows+total_raw_rows):.1f}% of all adaptive-format rows')
