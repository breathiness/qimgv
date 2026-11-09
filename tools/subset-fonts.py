#!/usr/bin/env python3
"""Subset the Source Han Sans CN faces qimgv ships.

The upstream release holds a ~8 MB OTF per weight, so the two faces the UI needs
are cut down to the characters it can show: the GB2312 repertoire plus every
string in the sources and translation files.

Usage:
    python tools/subset-fonts.py <dir-with-otf-files> [<output-dir>]

Input files are matched by name: anything containing "Regular" or "Bold".
Requires fonttools (pip install fonttools).
"""

import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUT = os.path.join(REPO, "qimgv", "res", "fonts")
WANTED = {"Regular": "SourceHanSansCN-Regular.otf", "Bold": "SourceHanSansCN-Bold.otf"}


def gb2312_chars() -> set[str]:
    chars = set()
    for high in range(0xB0, 0xF8):
        for low in range(0xA1, 0xFF):
            try:
                chars.add(bytes([high, low]).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    return chars


def repo_chars() -> set[str]:
    chars = set()
    for root, _dirs, files in os.walk(os.path.join(REPO, "qimgv")):
        for name in files:
            if not name.endswith((".ts", ".ui", ".cpp", ".h")):
                continue
            path = os.path.join(root, name)
            try:
                with open(path, encoding="utf-8") as handle:
                    chars.update(ch for ch in handle.read() if ord(ch) > 0x7F)
            except (UnicodeDecodeError, OSError):
                pass
    return chars


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    source_dir = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    os.makedirs(out_dir, exist_ok=True)

    charset = gb2312_chars() | repo_chars()
    text_path = os.path.join(out_dir, ".subset-chars.txt")
    with open(text_path, "w", encoding="utf-8") as handle:
        handle.write("".join(sorted(charset)))
    print(f"subset alphabet: {len(charset)} characters")

    for weight, out_name in WANTED.items():
        candidates = [f for f in os.listdir(source_dir)
                      if f.lower().endswith((".otf", ".ttf")) and weight.lower() in f.lower()]
        if not candidates:
            print(f"missing {weight} face in {source_dir}")
            continue
        src = os.path.join(source_dir, candidates[0])
        dst = os.path.join(out_dir, out_name)
        subprocess.run([sys.executable, "-m", "fontTools.subset", src,
                        f"--text-file={text_path}",
                        "--layout-features=*",
                        "--name-IDs=*",
                        "--output-file=" + dst], check=True)
        print(f"{os.path.basename(src)} -> {dst} "
              f"({os.path.getsize(src) // 1024} KB -> {os.path.getsize(dst) // 1024} KB)")
    os.remove(text_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
