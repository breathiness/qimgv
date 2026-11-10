"""Assemble a proposal .ico (preview only): pixel-art small frames + current smooth large frames."""
from PIL import Image
import io, os, struct

HERE = os.path.dirname(os.path.abspath(__file__))


def bmp_frame(im):
    """32bpp BGRA DIB + AND mask, the classic ICO frame layout."""
    w, h = im.size
    im = im.convert('RGBA')
    px = im.load()
    rows = []
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(w):
            r, g, b, a = px[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    xor = b''.join(rows)
    mask_stride = ((w + 31) // 32) * 4
    mask = bytes(mask_stride * h)          # all zero: alpha channel decides
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0,
                         len(xor) + len(mask), 0, 0, 0, 0)
    return header + xor + mask


def png_frame(im):
    buf = io.BytesIO()
    im.save(buf, format='PNG')
    return buf.getvalue()


def write_ico(path, frames):
    """frames: list of (size, PIL image, use_png)"""
    blobs = [(s, png_frame(im) if png else bmp_frame(im)) for s, im, png in frames]
    header = struct.pack('<HHH', 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    entries, data = b'', b''
    for (s, blob) in blobs:
        w = 0 if s == 256 else s
        entries += struct.pack('<BBBBHHII', w, w, 0, 0, 1, 32, len(blob), offset)
        data += blob
        offset += len(blob)
    with open(path, 'wb') as f:
        f.write(header + entries + data)
    print('wrote', path, os.path.getsize(path), 'bytes')


if __name__ == '__main__':
    frames = []
    for s in [16, 22, 24, 32, 36, 44, 48]:
        frames.append((s, Image.open(os.path.join(HERE, f'new-{s}.png')), False))
    for s in [64, 128, 256]:
        frames.append((s, Image.open(os.path.join(HERE, f'cur-{s}.png')), s == 256))
    write_ico(os.path.join(HERE, 'proposal.ico'), frames)
    # verify round-trip
    ico = Image.open(os.path.join(HERE, 'proposal.ico'))
    print('sizes:', sorted(set(x[0] for x in ico.info['sizes'])))
    for s in sorted(set(x[0] for x in ico.info['sizes'])):
        im = ico.ico.getimage((s, s))
        ref = Image.open(os.path.join(HERE, f'new-{s}.png' if s in (16, 22, 24, 32, 36, 44, 48) else f'cur-{s}.png')).convert('RGBA')
        same = im.convert('RGBA').tobytes() == ref.tobytes()
        print(s, im.size, 'matches png:', same)
