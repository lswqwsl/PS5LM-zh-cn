#!/usr/bin/env bash
# PS5LM 汉化版：烘 dashboard 的字形（ps5/app/assets/fonts/manrope-*.huifont）。
#
# 汉化版把字形换成了 Noto Sans SC（OFL 1.1）：界面文案是中文，聊天面板还要显示
# **模型生成的中文**，所以 regular 那个字重装了 GB2312 全集。真名见
# bake-fonts-cn.py 的说明；文件名保持 manrope-*.huifont，main.cpp 的加载路径不用动。
#
# 上游原来的做法（Manrope + european 字符集，只够英文界面用）留在这里备查：
#   bake_font manrope-$weight.ttf manrope-$weight.huifont 64 8 2048 european
#
# 依赖：fontTools（python3 -m pip install fonttools）+ 主机 clang++/g++。
#   CN_FONT=/path/to/NotoSansSC-VF.ttf scripts/bake-fonts.sh
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 "$root/scripts/bake-fonts-cn.py" "$@"
