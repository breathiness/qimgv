"""Regenerate the application icon set.

Pixel frames are drawn on their own grids by gen3; the 48 px and larger frames
come from qimgv.svg rendered once at 2048 px and then reduced with a Lanczos
filter on premultiplied alpha. Rendering the SVG at the target size with Cairo
samples the card shadow and the mark halo at that tiny size, which leaves a grey
halo around the white shapes; supersampling makes those edges crisp, and
premultiplying keeps transparent edges from picking up dark fringes.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gen3  # noqa: E402
import make_ico  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
APP = os.path.join(REPO, "qimgv", "res", "icons", "common", "logo", "app")
HICOLOR = os.path.join(REPO, "qimgv", "distrib", "hicolor")
SVG = os.path.join(APP, "qimgv.svg")
INKSCAPE = os.environ.get("INKSCAPE", r"C:\Program Files\Inkscape\bin\inkscape.exe")

# PNG assets the window icon and the hicolor theme use
PIXEL_PNGS = (16, 20, 22, 24, 30, 32, 36, 40, 44)
# frames inside qimgv.ico
ICO_PIXEL = (16, 20, 24, 30, 32, 40)
SVG_SIZES = (48, 60, 64, 80, 96, 120, 128, 160, 256)
SOURCE_SIZE = 2048


def downsample(im, size):
    """Lanczos resize through premultiplied alpha."""
    a = np.asarray(im.convert("RGBA")).astype(np.float32) / 255.0
    alpha = a[..., 3:4]
    prem = np.concatenate([a[..., :3] * alpha, alpha], axis=2)
    prem_im = Image.fromarray(np.clip(prem * 255 + 0.5, 0, 255).astype(np.uint8), "RGBA")
    small = np.asarray(prem_im.resize((size, size), Image.LANCZOS)).astype(np.float32) / 255.0
    sa = small[..., 3:4]
    rgb = np.where(sa > 1.0 / 255.0, small[..., :3] / np.maximum(sa, 1e-6), 0.0)
    out = np.concatenate([
        np.clip(rgb * 255 + 0.5, 0, 255).astype(np.uint8),
        np.clip(sa * 255 + 0.5, 0, 255).astype(np.uint8),
    ], axis=2)
    return Image.fromarray(out, "RGBA")


def render_source():
    dst = os.path.join(tempfile.mkdtemp(prefix="qimgv-app-icon-"), "qimgv-2048.png")
    subprocess.run([INKSCAPE, SVG, "--export-type=png", f"--export-filename={dst}",
                    f"--export-width={SOURCE_SIZE}", f"--export-height={SOURCE_SIZE}"],
                   check=True, capture_output=True)
    return Image.open(dst).convert("RGBA")


def main():
    source = render_source()
    frames = []
    for size in ICO_PIXEL:
        frames.append((size, gen3.render(size, gen3.SPECS[size], hole_colour=gen3.CLEAR), True))
    for size in SVG_SIZES:
        frames.append((size, downsample(source, size), True))
    make_ico.write_ico(os.path.join(APP, "qimgv.ico"), frames)
    print("ico frames:", [size for size, _image, _png in frames])

    for size in PIXEL_PNGS:
        image = gen3.render(size, gen3.SPECS[size], hole_colour=gen3.CLEAR)
        name = "22@2x.png" if size == 44 else f"{size}.png"
        image.save(os.path.join(APP, name))
        if size in (16, 22, 24, 32, 36):
            image.save(os.path.join(HICOLOR, f"{size}x{size}", "apps", "qimgv.png"))
    print("png assets updated")


if __name__ == "__main__":
    main()
