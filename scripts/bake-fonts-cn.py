#!/usr/bin/env python3
"""把中文字形烘成 dashboard 能加载的 SDF 字体：ps5/app/assets/fonts/manrope-*.huifont。

为什么文件名还叫 manrope：main.cpp 按这个名字加载，这里只换字形，不动代码。
为什么是两套字符集：
  * charset-ui.txt   —— 源码里出现过的界面字符（几百个）
  * charset-chat.txt —— 界面的字 + GB2312 全集（6763 汉字 + 682 符号）

聊天面板显示的是**模型生成的中文**，任意汉字都可能出现。只装界面那几百个字，
模型一开口就是一片 ????。

字体必须是 **TrueType 轮廓**（文件头 00 01 00 00）：stb_truetype 不认 CFF/OTF。
Noto Sans SC 的可变字体从这里下（OFL 1.1，见 ps5/app/assets/fonts/noto-sans-sc-LICENSE.txt）：

  https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf

用法：
    python3 scripts/bake-fonts-cn.py                  # 用 .deps/fonts/NotoSansSC-VF.ttf
    CN_FONT=/path/to/NotoSansSC-VF.ttf python3 scripts/bake-fonts-cn.py
    HOST_CXX=clang++ python3 scripts/bake-fonts-cn.py

依赖：fontTools（python3 -m pip install fonttools），一个主机 C++ 编译器（clang++/g++）。
烘焙结果直接进 git：控制台侧不需要字体工具链。
"""
import os
import pathlib
import shutil
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'ps5' / 'app' / 'assets' / 'fonts'
TMP = ROOT / 'build' / 'fonts'
CHARSETS = TMP / 'charsets'

# 文件名: (字重, 像素, SDF 范围, atlas 边长, 字符集)
# regular（聊天正文，模型生成的中文）：36px + GB2312 全集。36px 的 SDF 缩到界面
# 正文的 20~26px 反而比 64px 更贴合；6763 字 × 约 48² ≈ 15.6M 像素，正好塞满 4096²。
# semibold / display（标题、大数字）：64px，只装界面字集。
BAKES = {
    'manrope-500.huifont': (500, 36, 6, 4096, 'charset-chat'),
    'manrope-600.huifont': (600, 64, 8, 4096, 'charset-ui'),
    'manrope-200.huifont': (200, 64, 8, 4096, 'charset-ui'),
}

HEADER = struct.Struct('<IIHHfffffII')
GLYPH = struct.Struct('<IHHHHfff')


def die(message: str) -> 'None':
    print(message, file=sys.stderr)
    raise SystemExit(2)


def find_cxx() -> str:
    for name in (os.environ.get('HOST_CXX'), 'clang++', 'g++', 'c++'):
        if name and shutil.which(name):
            return name
    die('找不到主机 C++ 编译器：设置 HOST_CXX=clang++ 后重试')
    raise AssertionError


def build_baker() -> pathlib.Path:
    exe = TMP / ('bake_font.exe' if os.name == 'nt' else 'bake_font')
    src = ROOT / 'scripts' / 'font-baker' / 'bake_font.cpp'
    print(f'== 1. 编译字体烘焙器（{find_cxx()}）==')
    subprocess.run([find_cxx(), '-std=c++20', '-O2', '-w', str(src), '-o', str(exe)], check=True)
    return exe


def instance_weights(font: pathlib.Path) -> dict:
    try:
        from fontTools.ttLib import TTFont
        from fontTools.varLib.instancer import instantiateVariableFont
    except ImportError:
        die('找不到 fontTools：python3 -m pip install fonttools')
    print('== 3. 从可变字体取静态字重 ==')
    paths = {}
    for weight in sorted({b[0] for b in BAKES.values()}):
        out = TMP / f'noto-{weight}.ttf'
        instantiateVariableFont(TTFont(str(font)), {'wght': weight}).save(str(out))
        paths[weight] = out
        print(f'  {out.name}')
    return paths


def coverage(path: pathlib.Path, charset: pathlib.Path) -> tuple:
    """读回烘好的字体：字形数、atlas 尺寸，以及字符集里缺了哪些字。"""
    data = path.read_bytes()
    magic, version, w, h, px, rng, asc, desc, gap, nglyph, nkern = HEADER.unpack_from(data, 0)
    assert magic == 0x46505a50 and version == 1, f'{path.name}: 不是 huifont v1'
    codes = {GLYPH.unpack_from(data, HEADER.size + i * GLYPH.size)[0] for i in range(nglyph)}
    want = {ord(c) for c in charset.read_text(encoding='utf-8') if ord(c) >= 32}
    return (nglyph, w, h, px, len(want & codes), len(want), sorted(want - codes))


def main() -> int:
    font = pathlib.Path(os.environ.get('CN_FONT') or ROOT / '.deps' / 'fonts' / 'NotoSansSC-VF.ttf')
    if not font.exists():
        die(f'找不到字体: {font}\n'
            '下载：https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/'
            'NotoSansSC%5Bwght%5D.ttf\n'
            '放到 .deps/fonts/NotoSansSC-VF.ttf，或用 CN_FONT=... 指定')
    TMP.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)

    baker = build_baker()

    print('== 2. 生成两套字符集 ==')
    subprocess.run([sys.executable, str(ROOT / 'scripts' / 'gen-charset.py'), str(ROOT), str(CHARSETS)], check=True)

    weights = instance_weights(font)

    print('== 4. 烘焙 ==')
    failed = []
    for name, (weight, px, rng, atlas, charset) in BAKES.items():
        target = OUT / name
        subprocess.run([str(baker), str(weights[weight]), str(target), str(px), str(rng), str(atlas),
                        '@' + str(CHARSETS / f'{charset}.txt')], check=True)
        glyphs, w, h, bpx, got, want, missing = coverage(target, CHARSETS / f'{charset}.txt')
        status = 'ok' if not missing else f'缺 {len(missing)} 字'
        print(f'  {name}: {glyphs} 字形，atlas {w}x{h}，{bpx:g}px，{charset} 覆盖 {got}/{want}（{status}）')
        if missing:
            failed.append((name, ''.join(chr(c) for c in missing[:40])))

    print('== 5. 结果 ==')
    for name in BAKES:
        size = (OUT / name).stat().st_size
        print(f'  {name}: {size / 1048576:.1f} MiB')
    if failed:
        for name, chars in failed:
            print(f'字体缺字：{name}: {chars}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
