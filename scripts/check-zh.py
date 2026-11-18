#!/usr/bin/env python3
"""汉化后的自查：编码、换行、字面量是否被替换弄断、词条表是否干净。

用法：
    python3 scripts/check-zh.py [仓库根]

检查项：
  1. 所有文本文件都是合法 UTF-8 —— 一个坏字节在中文里就是屏幕上的一片乱码；
  2. 没有 CRLF —— 这个仓库的构建是 bash + cmake，CRLF 会让脚本和补丁直接跑不起来；
  3. C/C++ 源文件里的双引号成对，没有哪条字面量被替换弄成未闭合；
  4. 源码里不该再出现 ????
  5. translations-zh.csv 没有重复的 (file, en)，也没有 en == zh 的空行。

退出码非 0 就是有问题。
"""
import csv
import pathlib
import sys

TEXT = {'.cpp', '.hpp', '.h', '.c', '.py', '.md', '.html', '.csv', '.sh', '.txt', '.cmake', '.yml'}
QUOTES = {'.cpp', '.hpp', '.h', '.c'}
SKIP = {'.git', 'build', '.deps', 'third_party', 'node_modules', '__pycache__'}


def scan_quotes(text: str) -> tuple:
    """数双引号，顺带确认没有落单的引号或注释块。返回 (对数, 结束状态)。"""
    state = 'code'
    quotes = 0
    i = 0
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ''
        if state == 'code':
            if ch == '/' and nxt == '/':
                state = 'line'
                i += 2
                continue
            if ch == '/' and nxt == '*':
                state = 'block'
                i += 2
                continue
            if ch == '"':
                quotes += 1
                state = 'string'
            elif ch == "'":
                state = 'char'
            i += 1
            continue
        if state == 'string':
            if ch == '\\':
                i += 2
                continue
            if ch == '"':
                quotes += 1
                state = 'code'
            i += 1
            continue
        if state == 'char':
            if ch == '\\':
                i += 2
                continue
            if ch == "'":
                state = 'code'
            i += 1
            continue
        if state == 'line':
            if ch == '\n':
                state = 'code'
            i += 1
            continue
        if ch == '*' and nxt == '/':
            state = 'code'
            i += 2
            continue
        i += 1
    return quotes, state


def main() -> int:
    root = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[1]
    bad = 0
    checked = 0
    for path in sorted(root.rglob('*')):
        if not path.is_file() or SKIP & set(part for part in path.parts):
            continue
        if path.suffix.lower() not in TEXT and path.name not in {'CMakeLists.txt', 'Makefile'}:
            continue
        data = path.read_bytes()
        try:
            text = data.decode('utf-8')
        except UnicodeDecodeError as error:
            print(f'UTF-8: {path.relative_to(root)}: {error}')
            bad += 1
            continue
        checked += 1
        if b'\r\n' in data:
            print(f'CRLF: {path.relative_to(root)}')
            bad += 1
        suffix = path.suffix.lower()
        if suffix in QUOTES:
            quotes, state = scan_quotes(text)
            if quotes % 2 or state not in ('code', 'line'):
                print(f'QUOTES: {path.relative_to(root)}: {quotes} 个双引号，停在 {state}')
                bad += 1
        if suffix in QUOTES | {'.html'}:
            for marker in ('????', '\ufffd'):
                if marker in text:
                    print(f'PLACEHOLDER: {path.relative_to(root)}: {marker!r}')
                    bad += 1
    table = root / 'scripts' / 'translations-zh.csv'
    if table.exists():
        rows = list(csv.DictReader(table.open(encoding='utf-8', newline='')))
        keys = [(r['file'], r['en']) for r in rows]
        if len(keys) != len(set(keys)):
            print('CSV: (file, en) 有重复')
            bad += 1
        same = [r for r in rows if r['en'] == r['zh']]
        if same:
            print(f'CSV: {len(same)} 条译文和原文一样')
            bad += 1
        print(f'词条表: {len(rows)} 条')
    print(f'检查了 {checked} 个文本文件，{bad} 个问题')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
