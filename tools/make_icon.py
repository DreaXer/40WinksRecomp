# Builds a Windows .ico (16..256 px, PNG entries) and a 256 px .png from one square 8-bit RGBA PNG.
# Run from the project root: python -I tools/make_icon.py <source.png> <out.ico> [<out_256.png>]
# Downscaling averages the covered source pixels (premultiplied alpha), so small sizes stay clean.
import struct
import sys
import zlib

SIZES = [16, 24, 32, 48, 64, 128, 256]


def read_png(path):
    data = open(path, "rb").read()
    w, h, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", data[16:29])
    assert (depth, color, interlace) == (8, 6, 0), "need an 8-bit RGBA PNG: " + path
    idat, pos = b"", 8
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        if kind == b"IDAT":
            idat += data[pos + 8:pos + 8 + length]
        pos += 12 + length
    raw, stride, rows, prev = zlib.decompress(idat), w * 4, [], bytearray(w * 4)
    for y in range(h):
        filt, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if filt == 1:
                line[i] = (line[i] + a) & 0xFF
            elif filt == 2:
                line[i] = (line[i] + b) & 0xFF
            elif filt == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        rows.append(line)
        prev = line
    return w, h, rows


def encode_png(size, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    raw = b"".join(b"\0" + bytes(r) for r in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def downscale(w, rows, size):
    # Box filter: each output pixel averages the source pixels whose centers fall inside it.
    out = []
    for y in range(size):
        y0, y1 = y * w // size, max((y + 1) * w // size, y * w // size + 1)
        line = bytearray()
        for x in range(size):
            x0, x1 = x * w // size, max((x + 1) * w // size, x * w // size + 1)
            r = g = b = a = 0
            for sy in range(y0, y1):
                src = rows[sy]
                for sx in range(x0, x1):
                    pa = src[sx * 4 + 3]
                    r += src[sx * 4] * pa
                    g += src[sx * 4 + 1] * pa
                    b += src[sx * 4 + 2] * pa
                    a += pa
            n = (y1 - y0) * (x1 - x0)
            line += bytes([r // a, g // a, b // a, a // n]) if a else bytes(4)
        out.append(line)
    return out


w, h, rows = read_png(sys.argv[1])
assert w == h, "the source must be square"
pngs = [encode_png(s, downscale(w, rows, s)) for s in SIZES]
header = struct.pack("<HHH", 0, 1, len(pngs))
offset = 6 + 16 * len(pngs)
entries = b""
for s, p in zip(SIZES, pngs):
    entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(p), offset)
    offset += len(p)
open(sys.argv[2], "wb").write(header + entries + b"".join(pngs))
if len(sys.argv) > 3:
    open(sys.argv[3], "wb").write(pngs[-1])
