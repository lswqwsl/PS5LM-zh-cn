#!/usr/bin/env python3
"""汉化自查：每条译文的排版宽度和原文比一比（用烘好的字体量）。

用法：
    python3 scripts/fitcheck-zh.py [仓库根] [--size 24] [--top 40]

界面是按固定位置排版的（标题在 kTitleY、行高 92、提示行从右边数……），译文比原文宽太多
就会挤到邻居身上。中文在同样字号下通常比英文窄，所以这里只列出**反而更宽**的那些，
按宽度比从大到小排，方便在电视上看之前先把太长的缩短。
"""
import csv
import pathlib
import struct
import sys

HEADER = struct.Struct('<IIHHfffffII')   # 见 scripts/font-baker/font_format.hpp
GLYPH = struct.Struct('<IHHHHfff')


def load(path: pathlib.Path):
    data = path.read_bytes()
    magic, version, w, h, px, rng, asc, desc, gap, nglyph, nkern = HEADER.unpack_from(data, 0)
    assert magic == 0x46505a50, f'{path.name}: 不是 huifont v1'
    advances = {}
    for i in range(nglyph):
        cp, x, y, gw, gh, ox, oy, adv = GLYPH.unpack_from(data, HEADER.size + i * GLYPH.size)
        advances[cp] = adv
    return px, advances, (w, h)


def width(text: str, px: float, advances: dict, size: float) -> float:
    return sum(advances.get(ord(ch), 0) * size / px for ch in text)


def main() -> int:
    root = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 and not sys.argv[1].startswith('-') \
        else pathlib.Path(__file__).resolve().parents[1]
    size, top = 24.0, 40
    for i, arg in enumerate(sys.argv):
        if arg == '--size':
            size = float(sys.argv[i + 1])
        if arg == '--top':
            top = int(sys.argv[i + 1])
    px, advances, atlas = load(root / 'ps5' / 'app' / 'assets' / 'fonts' / 'manrope-500.huifont')
    rows = list(csv.DictReader((root / 'scripts' / 'translations-zh.csv').open(encoding='utf-8', newline='')))

    report = []
    for r in rows:
        we = width(r['en'], px, advances, size)
        wz = width(r['zh'], px, advances, size)
        if we > 0:
            report.append((wz / we, wz, we, r['file'], r['en'], r['zh']))
    report.sort(reverse=True)

    print(f'{len(report)} 条，按 {size:g}px 用烘好的 500 字重量（atlas {atlas[0]}x{atlas[1]}，源 {px:g}px）')
    print(f'{"宽度比":>7} {"中文 px":>8} {"英文 px":>8}  文件：en -> zh')
    for ratio, wz, we, file, en, zh in report[:top]:
        if ratio < 1.02:
            break
        print(f'{ratio:7.2f} {wz:8.1f} {we:8.1f}  {file}: {en[:52]!r} -> {zh[:30]!r}')
    wider = sum(1 for r in report if r[0] > 1.1)
    print(f'\n比英文宽 10% 以上的：{wider} 条')
    return 0


if __name__ == '__main__':
    sys.exit(main())
