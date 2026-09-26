"""Generate the native Floatlet icon from the same geometry as favicon.svg."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def rounded_rect(x, y, left, top, right, bottom, radius):
    cx = min(max(x, left + radius), right - radius)
    cy = min(max(y, top + radius), bottom - radius)
    return (x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2


def color(x, y):
    if not rounded_rect(x, y, 2, 2, 62, 62, 17):
        return (0, 0, 0, 0)
    base = (14, 16, 20, 255)
    ivory = (247, 246, 239, 255)
    violet = (174, 164, 237, 255)
    if rounded_rect(x, y, 17, 14, 29, 51, 6):
        return ivory
    if rounded_rect(x, y, 18, 14, 47, 26, 6):
        return ivory
    if rounded_rect(x, y, 18, 29, 41, 40, 5.5):
        return ivory
    if (x - 48) ** 2 + (y - 47) ** 2 <= 6 ** 2:
        return violet
    return base


frames = []
for n in SIZES:
    pixels = bytearray()
    for y in range(n - 1, -1, -1):
        for x in range(n):
            rgba = [0, 0, 0, 0]
            for sy in range(4):
                for sx in range(4):
                    sample = color((x + (sx + .5) / 4) * 64 / n,
                                   (y + (sy + .5) / 4) * 64 / n)
                    for channel in range(4):
                        rgba[channel] += sample[channel]
            pixels += bytes(round(rgba[channel] / 16) for channel in (2, 1, 0, 3))
    mask = bytes(((n + 31) // 32) * 4 * n)
    header = struct.pack('<IIIHHIIIIII', 40, n, n * 2, 1, 32, 0, len(pixels), 0, 0, 0, 0)
    frames.append((n, header + pixels + mask))

offset = 6 + 16 * len(frames)
directory = bytearray()
data = bytearray()
for n, frame in frames:
    directory += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(frame), offset)
    data += frame
    offset += len(frame)
(ROOT / 'src/app/island.ico').write_bytes(struct.pack('<HHH', 0, 1, len(frames)) + directory + data)
