#!/usr/bin/env python3
"""把一个 .huifont 里的字直接拼成 PNG，用来确认字形真的烘进图集了。

用法：
    python3 scripts/render-font-sample.py <字体.huifont> <输出.png> <字号> "第一行" "第二行" ...

它不经过任何字体文件：读 huifont 的表头、字形记录和 atlas，按 GPU 着色器同样的
(offset_x, offset_y, advance) 摆放每个字形。能读出中文，就说明聊天面板不会是一片 ????。
缺字会打印出来并返回非 0。
"""
import pathlib
import struct
import sys

from PIL import Image

# 见 scripts/font-baker/font_format.hpp
HEADER = struct.Struct('<IIHHfffffII')
GLYPH = struct.Struct('<IHHHHfff')


def main() -> int:
    font_path, out_path, size = sys.argv[1], sys.argv[2], float(sys.argv[3])
    lines = sys.argv[4:]
    data = pathlib.Path(font_path).read_bytes()
    magic, version, w, h, px, rng, asc, desc, gap, nglyph, nkern = HEADER.unpack_from(data, 0)
    assert magic == 0x46505a50, '不是 huifont v1'
    glyphs = {}
    for i in range(nglyph):
        cp, x, y, gw, gh, ox, oy, adv = GLYPH.unpack_from(data, HEADER.size + i * GLYPH.size)
        glyphs[cp] = (x, y, gw, gh, ox, oy, adv)
    atlas = data[HEADER.size + nglyph * GLYPH.size + nkern * 12:]

    scale = size / px
    line_h = int(size * 1.6)
    pad = int(size * 0.6)
    widest = max(sum((glyphs[ord(ch)][6] if ord(ch) in glyphs else size * 0.5) * scale for ch in line) for line in lines)
    image = Image.new('L', (int(widest) + 2 * pad, line_h * len(lines) + 2 * pad), 0)

    for row, line in enumerate(lines):
        pen_x = float(pad)
        baseline = pad + row * line_h + size * 1.15
        for ch in line:
            g = glyphs.get(ord(ch))
            if g is None:
                pen_x += size * 0.5
                continue
            x, y, gw, gh, ox, oy, adv = g
            if gw and gh:
                tile = Image.frombytes('L', (gw, gh), b''.join(atlas[(y + r) * w + x:(y + r) * w + x + gw]
                                                               for r in range(gh)))
                tile = tile.resize((max(1, int(gw * scale)), max(1, int(gh * scale))), Image.LANCZOS)
                image.paste(tile, (int(pen_x + ox * scale), int(baseline + oy * scale)))
            pen_x += adv * scale

    image.save(out_path)
    missing = sorted({ch for ch in ''.join(lines) if ord(ch) not in glyphs})
    print(f'{out_path}: {len(lines)} 行，{size:g}px，atlas {w}x{h}')
    if missing:
        print('缺字：' + ''.join(missing), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
