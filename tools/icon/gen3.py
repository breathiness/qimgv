"""Pixel-art qimgv icon v3: two colours (grey card + white), 44px and below."""
from PIL import Image, ImageDraw
import os

OUT = os.path.dirname(os.path.abspath(__file__))

WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)
GREY = (0x3E, 0x3E, 0x3E, 255)

# 16: the "I" bar is 1px so it stays hairline like the q stroke
SPECS = {
    16: dict(card=(1, 3, 14, 13), corner=[1],
             holes=(2, 13, 1, 2, [4, 7, 10]),
             glyph=(5, 5, 5, 5, 1, 2, 1, 1, 7)),
    # 20 px: Explorer asks for 20 physical pixels for its 16 px icons at
    # 125 % scaling; without a frame at that size the 22 px art was resampled.
    20: dict(card=(1, 3, 18, 16), corner=[1],
             holes=(2, 16, 2, 2, [5, 8, 11, 14]),
             glyph=(5, 6, 7, 7, 2, 1, 1, 2, 8)),
    22: dict(card=(1, 4, 20, 18), corner=[1],
             holes=(2, 18, 2, 2, [6, 9, 12, 15]),
             glyph=(6, 7, 8, 7, 2, 2, 1, 2, 9)),
    24: dict(card=(1, 4, 22, 19), corner=[1],
             holes=(2, 20, 2, 2, [5, 8, 11, 14, 17]),
             glyph=(6, 8, 9, 7, 2, 2, 1, 2, 9)),
    # 30 px: 24 logical pixels at 125 % scaling
    30: dict(card=(1, 5, 28, 24), corner=[2, 1],
             holes=(3, 26, 2, 2, [7, 11, 15, 19]),
             glyph=(8, 9, 11, 10, 3, 2, 1, 3, 12)),
    32: dict(card=(1, 5, 30, 26), corner=[2, 1],
             holes=(3, 27, 3, 2, [7, 11, 15, 19, 23]),
             glyph=(8, 10, 11, 10, 3, 2, 2, 3, 12)),
    36: dict(card=(2, 6, 33, 29), corner=[2, 1],
             holes=(3, 30, 3, 2, [8, 12, 16, 20, 24]),
             glyph=(9, 11, 13, 11, 3, 3, 2, 3, 14)),
    # 40 px: 32 logical pixels at 125 % scaling
    40: dict(card=(2, 7, 37, 33), corner=[3, 2, 1],
             holes=(4, 33, 3, 3, [10, 15, 20, 25, 30]),
             glyph=(10, 12, 14, 13, 4, 3, 2, 4, 16)),
    44: dict(card=(2, 7, 41, 36), corner=[2, 1],
             holes=(4, 37, 3, 3, [11, 16, 21, 26, 31]),
             glyph=(11, 14, 16, 14, 4, 3, 2, 4, 17)),
}


def render(S, spec, hole_colour=WHITE, grey=GREY):
    im = Image.new('RGBA', (S, S), CLEAR)
    d = ImageDraw.Draw(im)
    x0, y0, x1, y1 = spec['card']
    corner = spec['corner']
    for y in range(y0, y1 + 1):
        off = min(y - y0, y1 - y)
        inset = corner[off] if off < len(corner) else 0
        d.rectangle([x0 + inset, y, x1 - inset, y], fill=grey)
    hxl, hxr, hw, hh, ys = spec['holes']
    for y in ys:
        for hx in (hxl, hxr):
            d.rectangle([hx, y, hx + hw - 1, y + hh - 1], fill=hole_colour)
    gx, gy, qw, ring, st, tail, gap, iw, gh = spec['glyph']
    m = Image.new('L', (S, S), 0)
    md = ImageDraw.Draw(m)
    md.rectangle([gx, gy, gx + qw - 1, gy + ring - 1], fill=255)
    md.rectangle([gx + st, gy + st, gx + qw - 1 - st, gy + ring - 1 - st], fill=0)
    md.rectangle([gx + qw - st, gy, gx + qw - 1, gy + ring - 1 + tail], fill=255)
    md.rectangle([gx + qw + gap, gy, gx + qw + gap + iw - 1, gy + gh - 1], fill=255)
    # the q counter stays white-on-grey; punch it back to the card colour
    counter = Image.new('L', (S, S), 0)
    cd = ImageDraw.Draw(counter)
    cd.rectangle([gx + st, gy + st, gx + qw - 1 - st, gy + ring - 1 - st], fill=255)
    im.paste(Image.new('RGBA', (S, S), grey), (0, 0), counter)
    im.paste(Image.new('RGBA', (S, S), WHITE), (0, 0), m)
    return im


if __name__ == '__main__':
    for s, spec in SPECS.items():
        render(s, spec).save(os.path.join(OUT, f'v3-{s}.png'))
    print('ok')
