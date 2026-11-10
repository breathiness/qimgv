"""Generate the per-format icon set (pixel <=40, SVG 48+), .ico files and the C++/.rc tables."""
import os
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw
from fmt_round3 import F3, F4, F5

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "qimgv"))
ICONS = os.path.join(REPO, "res", "icons", "formats")
WORK = tempfile.mkdtemp(prefix="qimgv-format-icons-")

# Run from anywhere: writes qimgv/res/icons/formats/*.ico, qimgv/fileformats.h
# and qimgv/fileformats.rc, and needs Inkscape for the 48 px and larger frames.
INKSCAPE = r"C:\Program Files\Inkscape\bin\inkscape.exe"
SVG_SIZES = (48, 60, 64, 80, 96, 120, 128, 160, 256)
SOURCE_SIZE = 2048
WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)
GREY = (0x42, 0x42, 0x42, 255)

# name, extensions, code3, code4, label, colour
FORMATS = [
    ("jpg",  ["jpg", "jpeg"], "JPG", "JPEG", "JPEG image", "#E0771B"),
    ("png",  ["png"],         "PNG", "PNG",  "PNG image", "#2E9E4F"),
    ("gif",  ["gif"],         "GIF", "GIF",  "GIF image", "#8A3FD1"),
    ("webp", ["webp"],        "WBP", "WEBP", "WebP image", "#2B6FD6"),
    ("bmp",  ["bmp"],         "BMP", "BMP",  "BMP image", "#0F8A8A"),
    ("tif",  ["tif", "tiff"], "TIF", "TIFF", "TIFF image", "#7A5C3E"),
    ("ico",  ["ico"],         "ICO", "ICO",  "Icon", "#5B5BD6"),
    ("svg",  ["svg", "svgz"], "SVG", "SVG",  "SVG image", "#D19A1E"),
    ("avif", ["avif"],        "AVF", "AVIF", "AVIF image", "#1E9E9E"),
    ("heic", ["heic", "heif"],"HEC", "HEIC", "HEIF image", "#3E9E6E"),
    ("jxl",  ["jxl"],         "JXL", "JXL",  "JPEG XL image", "#7A3FD1"),
    ("psd",  ["psd"],         "PSD", "PSD",  "Photoshop document", "#1F4E9E"),
    ("exr",  ["exr"],         "EXR", "EXR",  "OpenEXR image", "#4E6E8A"),
    ("hdr",  ["hdr"],         "HDR", "HDR",  "HDR image", "#5E7E9A"),
    ("dds",  ["dds"],         "DDS", "DDS",  "DirectDraw surface", "#456A8A"),
    ("qoi",  ["qoi"],         "QOI", "QOI",  "QOI image", "#D14E8A"),
    ("pfm",  ["pfm"],         "PFM", "PFM",  "Portable float map", "#8A5ED1"),
    ("fits", ["fits", "fit"], "FIT", "FITS", "FITS image", "#2A4E8A"),
    ("raw",  ["cr2", "cr3", "nef", "arw", "dng", "orf", "rw2", "raf"], "RAW", "RAW", "Camera raw image", "#6E6E6E"),
    ("kra",  ["kra"],         "KRA", "KRA",  "Krita document", "#D1604E"),
    ("ora",  ["ora"],         "ORA", "ORA",  "OpenRaster image", "#C2604E"),
    ("cbz",  ["cbz"],         "CBZ", "CBZ",  "Comic archive", "#C28A1E"),
    ("pdf",  ["pdf"],         "PDF", "PDF",  "PDF document", "#B32D2D"),
    ("ai",   ["ai"],          "AI",  "AI",   "Illustrator document", "#C2601E"),
    ("dxf",  ["dxf"],         "DXF", "DXF",  "DXF drawing", "#4E8A6E"),
    ("dwg",  ["dwg"],         "DWG", "DWG",  "DWG drawing", "#3E7A5E"),
    ("mp4",  ["mp4", "m4v"],  "MP4", "MP4",  "MP4 video", "#D14545"),
    ("mkv",  ["mkv"],         "MKV", "MKV",  "Matroska video", "#3B5BDB"),
    ("webm", ["webm"],        "WBM", "WEBM", "WebM video", "#6B4ED6"),
    ("mov",  ["mov"],         "MOV", "MOV",  "QuickTime video", "#C24ED1"),
    ("avi",  ["avi"],         "AVI", "AVI",  "AVI video", "#D16A2B"),
    ("flv",  ["flv"],         "FLV", "FLV",  "Flash video", "#D18A2B"),
    ("ts",   ["ts", "mts"],   "TS",  "TS",   "Transport stream", "#2B8AD1"),
    ("m2ts", ["m2ts"],        "M2T", "M2TS", "Blu-ray stream", "#3B9ED1"),
    ("stl",  ["stl"],         "STL", "STL",  "STL model", "#6D4C41"),
    ("obj",  ["obj"],         "OBJ", "OBJ",  "OBJ model", "#7A5C4E"),
    ("ply",  ["ply"],         "PLY", "PLY",  "PLY model", "#8A6A4E"),
    ("gltf", ["gltf", "glb"], "GLT", "GLTF", "glTF model", "#4E7A5E"),
    ("fbx",  ["fbx"],         "FBX", "FBX",  "FBX model", "#5E6A4E"),
    ("3ds",  ["3ds"],         "3DS", "3DS",  "3DS model", "#6E5E8A"),
    ("3mf",  ["3mf"],         "3MF", "3MF",  "3MF model", "#7A8A4E"),
    ("dae",  ["dae"],         "DAE", "DAE",  "COLLADA model", "#5E8A7A"),
]

FONTS = {'3': F3, '4': F4, '5': F5}
GW = {'3': 3, '4': 4, '5': 5}

# pixel layout per size: card, corner, shell top, teeth (x, ys, w, h), font, tx, ty
PIX = {
    16: dict(card=(1, 1, 14, 14), corner=[1], shell=7,
             teeth=(2, 13, [2], 1, 2), font='3', tx=2, ty=8),
    # 20 px exists because Explorer asks for 20 physical pixels for its 16 px
    # icons at 125 % scaling. Without a frame at that size Windows resampled the
    # 24 px art and the icons looked blurry on such displays.
    20: dict(card=(1, 1, 18, 18), corner=[1], shell=10,
             teeth=(2, 17, [3, 6], 1, 2), font='4', tx=2, ty=12),
    24: dict(card=(1, 1, 22, 22), corner=[1], shell=11,
             teeth=(2, 20, [3, 7], 2, 2), font='4', tx=5, ty=14),
    # 30/40 px: 24/32 logical pixels at 125 % scaling
    30: dict(card=(1, 1, 28, 28), corner=[2, 1], shell=13,
             teeth=(2, 26, [4, 8], 2, 2), font='5', tx=6, ty=17),
    32: dict(card=(1, 1, 30, 30), corner=[2, 1], shell=14,
             teeth=(3, 27, [4, 9], 2, 2), font='5', tx=8, ty=19),
    40: dict(card=(2, 2, 37, 37), corner=[3, 2, 1], shell=17,
             teeth=(4, 33, [5, 11], 3, 3), font='5', tx=11, ty=24),
}


def rgb(hexcolour):
    h = hexcolour.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), 255)


def pixel(size, code, colour):
    L = PIX[size]
    im = Image.new('RGBA', (size, size), CLEAR)
    d = ImageDraw.Draw(im)
    x0, y0, x1, y1 = L['card']

    def inset(y):
        off = min(y - y0, y1 - y)
        return L['corner'][off] if off < len(L['corner']) else 0

    for y in range(y0, L['shell'] + 2):
        s = inset(y)
        d.rectangle([x0 + s, y, x1 - s, y], fill=GREY)
    # The perforations are holes in the film card, not white rivets: punch them
    # out so the background shows through, like the vector frames do.
    hxl, hxr, ys, hw, hh = L['teeth']
    for hy in ys:
        for hx in (hxl, hxr):
            d.rectangle([hx, hy, hx + hw - 1, hy + hh - 1], fill=CLEAR)
    for y in range(L['shell'], y1 + 1):
        s = inset(y)
        d.rectangle([x0 + s, y, x1 - s, y], fill=colour)
    font = L['font']
    tw = len(code) * GW[font] + (len(code) - 1)
    cx = (x0 + x1 + 1 - tw) // 2
    for ch in code:
        for r, bits in enumerate(FONTS[font][ch]):
            for c, bit in enumerate(bits):
                if bit == '1':
                    d.rectangle([cx + c, L['ty'] + r, cx + c, L['ty'] + r], fill=WHITE)
        cx += GW[font] + 1
    return im


def svg(code, colour):
    scale = 0.82 if len(code) >= 4 else 1.0
    fs = int(108 * scale)
    holes = ''.join(
        f'<rect x="{x}" y="{y}" width="38" height="34" rx="8" fill="#000000"/>'
        for y in (44, 96, 148, 200, 252) for x in (46, 428))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
  <defs>
    <filter id="drop" x="-20%" y="-20%" width="140%" height="140%" color-interpolation-filters="sRGB">
      <feGaussianBlur in="SourceAlpha" stdDeviation="7" result="blur"/>
      <feOffset in="blur" dx="0" dy="6" result="off"/>
      <feFlood flood-color="#000000" flood-opacity="0.38" result="col"/>
      <feComposite in="col" in2="off" operator="in" result="shadow"/>
      <feMerge><feMergeNode in="shadow"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <mask id="holes" maskUnits="userSpaceOnUse" x="0" y="0" width="512" height="512">
      <rect x="0" y="0" width="512" height="512" fill="#ffffff"/>
      {holes}
    </mask>
  </defs>
  <rect x="24" y="24" width="464" height="306" rx="26"
        fill="#424242" mask="url(#holes)" filter="url(#drop)"/>
  <g transform="translate(0,-96)">
    <g transform="translate(62.250,56.000) scale(0.78125)"><g transform="scale(60.4833)">
      <path fill="#ffffff" d="M 5.82083,2.11667 H 6.87916 V 6.35001 H 5.82083 Z M 1.32292,5.55625 H 2.38125 V 2.11666 H 1.32292 Z M 2.38125,5.55624 h 1.85208 V 4.49791 H 2.38125 Z M 2.38125,3.17501 h 1.85208 V 2.11667 H 2.38125 Z M 4.23333,6.35001 h 1.05833 V 2.11667 H 4.23333 Z"/>
    </g></g>
  </g>
  <rect x="16" y="300" width="480" height="188" rx="28" fill="{colour}" filter="url(#drop)"/>
  <rect x="16" y="302" width="480" height="4" rx="2" fill="#ffffff" opacity="0.14"/>
  <text x="256" y="392" font-family="Segoe UI, Arial, sans-serif" font-weight="bold"
        font-size="{fs}" fill="#ffffff" text-anchor="middle" dominant-baseline="central">{code}</text>
</svg>
'''


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


def ico(frames):
    import io, struct
    blobs = []
    for im in frames:
        buf = io.BytesIO()
        im.save(buf, format='PNG')
        blobs.append((im.width, buf.getvalue()))
    head = struct.pack('<HHH', 0, 1, len(blobs))
    off = 6 + 16 * len(blobs)
    entries, data = b'', b''
    for s, blob in blobs:
        w = 0 if s == 256 else s
        entries += struct.pack('<BBBBHHII', w, w, 0, 0, 1, 32, len(blob), off)
        data += blob
        off += len(blob)
    return head + entries + data


def main():
    os.makedirs(ICONS, exist_ok=True)
    os.makedirs(os.path.join(WORK, 'fmt-work'), exist_ok=True)
    header = ['#pragma once', '// Generated by tools/format-icons/gen_formats.py', '']
    rows = []
    rc = ['// Generated by tools/format-icons/gen_formats.py', '#include <winver.h>', '']
    idx = 0
    for name, exts, c3, c4, label, colour in FORMATS:
        idx += 1
        icon_id = 2000 + idx
        colour_rgba = rgb(colour)
        frames = [pixel(s, c3, colour_rgba) for s in (16, 20, 24, 30, 32, 40)]
        svg_path = os.path.join(WORK, f'fmt-{name}.svg')
        with open(svg_path, 'w', encoding='utf-8') as f:
            f.write(svg(c4, colour))
        # One 2048 px render per format, then Lanczos on premultiplied alpha:
        # rasterising the vector at 48-256 samples the card shadow and the mark
        # halo at that size and leaves a grey halo around the white shapes.
        big = os.path.join(WORK, 'fmt-work', f'{name}-{SOURCE_SIZE}.png')
        subprocess.run([INKSCAPE, svg_path, '--export-type=png',
                        f'--export-filename={big}', f'--export-width={SOURCE_SIZE}',
                        f'--export-height={SOURCE_SIZE}'], check=True, capture_output=True)
        source = Image.open(big).convert('RGBA')
        frames += [downsample(source, s) for s in SVG_SIZES]
        blob = ico(frames)
        dst = os.path.join(ICONS, f'{name}.ico')
        with open(dst, 'wb') as f:
            f.write(blob)
        rc.append(f'{icon_id} ICON DISCARDABLE "res/icons/formats/{name}.ico"')
        for ext in exts:
            rows.append((ext, label, icon_id))
        print('built', name, len(blob))

    rows.sort()
    header.append('struct FileFormatEntry { const char *ext; const char *label; int iconId; };')
    header.append('static const FileFormatEntry kFileFormats[] = {')
    for ext, label, icon_id in rows:
        header.append(f'    {{ "{ext}", "{label}", {icon_id} }},')
    header.append('};')
    header.append(f'static const int kFileFormatCount = {len(rows)};')
    header.append('')
    with open(os.path.join(REPO, 'fileformats.h'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(header))
    with open(os.path.join(REPO, 'fileformats.rc'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(rc) + '\n')
    print('extensions', len(rows), 'icons', len(FORMATS))


if __name__ == '__main__':
    main()
