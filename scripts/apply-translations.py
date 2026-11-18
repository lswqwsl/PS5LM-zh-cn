#!/usr/bin/env python3
"""把 scripts/translations-zh.csv 里的英文界面文案换成简体中文。

用法：
    python3 scripts/apply-translations.py <源码根> [词条表] [--dry-run]

规则：
  * 只替换**带双引号的完整字面量** —— `"Load"` 不会误伤 `"Loading"`；
  * 同一文件内全部命中都替换（译文一致，不会同一个词两种译法）；
  * 源码里用 `\\xNN` 转义写的中文/符号（如 "  \\xC2\\xB7  "）也认得；
  * 一条都没命中的列在最后，用来抓错字和漏项；有漏项时退出码为 1；
  * 改完把原文件留一份 `<文件>.en.bak`（已在 .gitignore 里）。

⚠ 这个脚本**只做字面量替换**。像 wrap() 的断行、把 `"decision model ("`
   前缀剥掉这类逻辑改动不在词条表里，那些是单独改的代码（见 docs/LOCALIZATION.md）。
"""
import csv
import shutil
import sys
from pathlib import Path

SKIP_DIRS = {'third_party', 'build', '.git', '.deps', 'assets', 'node_modules', 'fleet', 'site', 'tests'}


def c_escaped(text: str) -> str:
    """UTF-8 文本写成 C 字面量：非 ASCII 转成 \\xNN，引号与反斜杠转义。"""
    out = []
    for byte in text.encode('utf-8'):
        ch = chr(byte)
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif 0x20 <= byte < 0x7f:
            out.append(ch)
        else:
            out.append(f'\\x{byte:02X}')
    return ''.join(out)


def forms(text: str) -> list[str]:
    """同一个字面量的两种写法：原样 UTF-8，以及 \\xNN 转义。"""
    raw = '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'
    escaped = '"' + c_escaped(text) + '"'
    return [raw] if raw == escaped else [raw, escaped]


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    if not args:
        print(__doc__)
        return 2
    root = Path(args[0]).resolve()
    table = Path(args[1]).resolve() if len(args) > 1 else Path(__file__).with_name('translations-zh.csv')
    dry = '--dry-run' in sys.argv
    if not table.exists():
        print(f'找不到词条表: {table}', file=sys.stderr)
        return 2

    rows = []
    with table.open(encoding='utf-8', newline='') as fh:
        for rec in csv.DictReader(fh):
            name = (rec.get('file') or '').strip()
            en, zh = rec.get('en') or '', rec.get('zh') or ''
            if name and en and zh:
                rows.append((name, en, zh))
    print(f'词条: {len(rows)}')

    by_file: dict[str, list[tuple[str, str]]] = {}
    for name, en, zh in rows:
        by_file.setdefault(name, []).append((en, zh))

    misses: list[tuple[str, str]] = []
    total = 0
    for name, pairs in by_file.items():
        target = None
        for cand in sorted(root.rglob(name)):
            if any(part in SKIP_DIRS for part in cand.parts) or not cand.is_file():
                continue
            target = cand
            break
        if target is None:
            misses.append((name, '(文件没找到)'))
            continue
        text = target.read_text(encoding='utf-8')
        original = text
        replaced = 0
        for en, zh in pairs:
            hit = 0
            for needle in forms(en):
                hit = text.count(needle)
                if hit:
                    text = text.replace(needle, '"' + zh.replace('"', '\\"') + '"')
                    break
            if hit == 0:
                misses.append((name, en))
                continue
            replaced += hit
            total += hit
        if text != original and not dry:
            bak = target.with_suffix(target.suffix + '.en.bak')
            if not bak.exists():
                shutil.copy2(target, bak)
            target.write_text(text, encoding='utf-8', newline='\n')
        print(f'{name}: 替换 {replaced} 处{"（dry-run）" if dry else ""}')

    print(f'\n合计替换 {total} 处')
    if misses:
        print(f'\n未命中 {len(misses)} 条:')
        for name, en in misses:
            print(f'  [{name}] {en}')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
