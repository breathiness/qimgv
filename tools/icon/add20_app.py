"""Add a 20 px pixel frame to the application icon.

Same reason as the format icons: at 125 % scaling Windows asks for 20 physical
pixels for a 16 px icon and resampled the 22 px art, which looked soft.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gen3  # noqa: E402
import make_ico  # noqa: E402

ICO = os.path.abspath(os.path.join(HERE, "..", "..", "qimgv", "res", "icons",
                                   "common", "logo", "app", "qimgv.ico"))

SPEC20 = dict(card=(1, 3, 18, 17), corner=[1],
              holes=(2, 17, 1, 2, [4, 7, 10, 13]),
              glyph=(5, 6, 7, 6, 2, 2, 1, 2, 8))


def read_frames(path):
    data = open(path, "rb").read()
    import struct
    _r, _t, count = struct.unpack("<HHH", data[:6])
    frames = []
    for i in range(count):
        w, h, colors, r, planes, bpp, size, off = struct.unpack("<BBBBHHII",
                                                                 data[6 + i * 16:6 + (i + 1) * 16])
        side = w or 256
        payload = data[off:off + size]
        if payload[:8] == b"\x89PNG\r\n\x1a\n":
            import io
            im = Image.open(io.BytesIO(payload)).convert("RGBA")
        else:
            im = None  # BMP frames are rebuilt from their size below
        frames.append((side, im, payload))
    return frames


def main():
    image = gen3.render(20, SPEC20)
    image.save(os.path.join(HERE, "app-20.png"))
    print("rendered 20 px frame")

    frames = read_frames(ICO)
    # the small frames are plain pixel art; rebuild each one from gen3 so the
    # whole set stays consistent, then insert 20 in size order
    rebuilt = []
    for side, im, payload in frames:
        if payload[:8] == b"\x89PNG\r\n\x1a\n":
            rebuilt.append((side, im, True))
        elif side in gen3.SPECS:
            rebuilt.append((side, gen3.render(side, gen3.SPECS[side]), False))
        else:
            # smooth 48+ frames stay as they are
            import io
            rebuilt.append((side, Image.open(io.BytesIO(payload)), True))
    rebuilt.append((20, image, False))
    rebuilt.sort(key=lambda f: f[0])

    tmp = ICO + ".new"
    make_ico.write_ico(tmp, rebuilt)
    os.replace(tmp, ICO)
    print("frames now:", [f[0] for f in rebuilt])


if __name__ == "__main__":
    main()
