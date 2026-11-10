"""Check every file-format icon shipping in the installed exe.

For each icon group (ids 2001..2042) it verifies the frame ladder, extracts the
20 px (125 % small icons) and 60 px (125 % medium icons) frames, and lays them
out as a labelled contact sheet so the real rendered result can be inspected.
"""
import io
import os
import re
import struct
import sys
import tempfile

import pefile
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
# usage: check_shipped_icons.py [exe] [outdir]
EXE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "build8", "qimgv", "qimgv.exe")
HEADER = os.path.join(REPO, "qimgv", "fileformats.h")
OUT = sys.argv[2] if len(sys.argv) > 2 else tempfile.gettempdir()

EXPECTED = [16, 20, 24, 30, 32, 40, 48, 60, 64, 80, 96, 120, 128, 160, 256]


def load_groups(path):
    pe = pefile.PE(path)
    groups, images = {}, {}
    for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if entry.id not in (3, 14):
            continue
        for name in entry.directory.entries:
            if name.id is None:
                continue
            for lang in name.directory.entries:
                data = lang.data.struct
                blob = pe.get_data(data.OffsetToData, data.Size)
                if entry.id == 14:
                    groups[name.id] = blob
                else:
                    images[name.id] = blob
    return groups, images


def frames_of(group_blob):
    count = struct.unpack("<H", group_blob[4:6])[0]
    out = []
    for i in range(count):
        w, h, colors, r, planes, bpp, size, iid = struct.unpack(
            "<BBBBHHIH", group_blob[6 + i * 14:6 + (i + 1) * 14])
        out.append((w or 256, size, iid))
    return out


def bitmap(payload):
    if payload[:8] == b"\x89PNG\r\n\x1a\n":
        return Image.open(io.BytesIO(payload)).convert("RGBA")
    # 32 bpp DIB
    header_size = struct.unpack("<I", payload[:4])[0]
    w, h = struct.unpack("<ii", payload[4:12])
    px = payload[header_size:]
    im = Image.new("RGBA", (w, h // 2))
    data = []
    for y in range(h // 2):
        row = px[(h // 2 - 1 - y) * w * 4:(h // 2 - y) * w * 4]
        for x in range(w):
            b, g, r, a = row[x * 4:x * 4 + 4]
            data.append((r, g, b, a))
    im.putdata(data)
    return im


def read_header():
    text = open(HEADER, encoding="utf-8").read()
    rows = re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)",\s*(\d+)\s*\}', text)
    return [(ext, label, int(icon_id)) for ext, label, icon_id in rows]


def main():
    groups, images = load_groups(EXE)
    table = read_header()
    print(f"extensions in table: {len(table)}, icon groups in exe: {len(groups)}")

    used = sorted({icon_id for _, _, icon_id in table})
    problems = []

    for icon_id in used:
        blob = groups.get(icon_id)
        if blob is None:
            problems.append(f"{icon_id}: group missing")
            continue
        frames = frames_of(blob)
        sizes = [f[0] for f in frames]
        if sizes != EXPECTED:
            problems.append(f"{icon_id}: frames {sizes}")
        for size, nbytes, iid in frames:
            payload = images.get(iid)
            if not payload or len(payload) != nbytes:
                problems.append(f"{icon_id}@{size}: image {iid} missing/short")
                continue
            try:
                im = bitmap(payload)
            except Exception as exc:  # noqa: BLE001
                problems.append(f"{icon_id}@{size}: decode failed {exc}")
                continue
            if im.size != (size, size):
                problems.append(f"{icon_id}@{size}: size {im.size}")
            visible = sum(1 for p in im.getdata() if p[3] > 200)
            if visible < (size * size) * 0.15:
                problems.append(f"{icon_id}@{size}: only {visible} opaque px")

    print("frame/decoding problems:", len(problems))
    for line in problems[:20]:
        print("  !", line)

    # contact sheets from the real frames
    first_ext = {}
    for ext, label, icon_id in table:
        first_ext.setdefault(icon_id, ext)

    def sheet(size, tag, columns=7):
        cell_w, cell_h = max(size, 64), size + 18
        rows = (len(used) + columns - 1) // columns
        canvas = Image.new("RGBA", (cell_w * columns, cell_h * rows), (32, 32, 34, 255))
        draw = ImageDraw.Draw(canvas)
        for idx, icon_id in enumerate(used):
            frames = frames_of(groups[icon_id])
            payload = images[dict((s, i) for s, _, i in frames)[size]]
            im = bitmap(payload)
            x = (idx % columns) * cell_w
            y = (idx // columns) * cell_h
            canvas.alpha_composite(im, (x + (cell_w - size) // 2, y + 4))
            draw.text((x + 4, y + size + 6), first_ext[icon_id], fill=(200, 200, 200, 255))
        path = rf"{OUT}\icons-sheet-{tag}.png"
        canvas.save(path)
        print("wrote", path, canvas.size)

    sheet(20, "20px")
    sheet(60, "60px")

    # 100 % and 125 % size mapping: which frame each request resolves to
    for dpr, name in ((1.0, "100 %"), (1.25, "125 %")):
        print(f"--- {name} display ---")
        for logical in (16, 24, 32, 48, 96, 256):
            want = round(logical * dpr)
            best = min(EXPECTED, key=lambda s: (abs(s - want), -s))
            state = "exact" if best == want else f"resampled from {best}"
            print(f"  {logical:3d} logical -> {want:3d} device -> {state}")


if __name__ == "__main__":
    sys.exit(main())
