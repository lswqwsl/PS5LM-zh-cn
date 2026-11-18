# 汉化说明

这是 [cobanov/PS5LM](https://github.com/cobanov/PS5LM) 的简体中文汉化分支。做法参照了
[Ykls53/PS5LM](https://github.com/Ykls53/PS5LM) 的汉化版（词条表 + 字符集 + 重烘字体），
但覆盖的是**上游最新版**：多出来的 fleet、调试报告、`/v1/responses`、图像输入、决策模型、
下载队列这些页面和文案都在里面。

## 一眼看懂改了什么

| 东西 | 动了没有 | 说明 |
|---|---|---|
| 电视端界面（主页、模型、API、控制台、设置） | ✅ 全中文 | `ps5/app/dashboard.cpp` 的词条表替换 |
| 提示行、对话框、忙碌页 | ✅ 全中文 | 同上 |
| 通知（右上角弹条）、重启/加载时的说明 | ✅ 全中文 | `ps5/app/main.cpp` |
| 模型规划文案（「64k 上下文，q4_0 缓存：11.3 / 11.4 GB」） | ✅ 全中文 | `ps5/app/model_plan.cpp` |
| 模型库、搜索、下载状态与失败原因 | ✅ 全中文 | `ps5/app/market.cpp` |
| 「推荐」列表里的模型介绍 | ✅ 全中文 | `ps5/app/suggested.hpp` |
| 请求面板里的「3 个问题」「是 0.87」「错误：…」 | ✅ 全中文 | `ps5/app/activity.cpp` |
| 硬盘名（内置存储 / 应用存储 / M.2 硬盘 / USB 硬盘） | ✅ 全中文 | `stats.cpp`、`storage.cpp`，dashboard 里的比较也一起改了 |
| 浏览器聊天页、模型库页 | ✅ 全中文 | `ps5/webui/index.html`、`ps5/ps5lm/library.html` |
| README | ✅ 全中文 | 其余 `docs/*.md`、`site/`、`fleet/control.html` 保留英文 |
| 字体 | ✅ 换成 Noto Sans SC | `ps5/app/assets/fonts/manrope-*.huifont` 重新烘焙见下 |
| `app.log` / `llama.log` 日志行、llama-server 参数、API 路径、JSON 字段 | ❌ 保留英文 | 这些是给人和工具抓错误用的，翻了反而搜不到 |

## 为什么必须重烘字体

`main.cpp` 从 `assets/fonts` 加载 `manrope-200/500/600.huifont`（SDF 字体图集），一套字体只有
烘进去的字形。上游烘的是 Manrope + 欧洲字符集，里面一个汉字都没有 —— 界面文案换成中文后，
屏幕上就是一片 `????`。

汉化版把这三个文件换成了 **Noto Sans SC**（OFL 1.1）烘出来的图集，文件名不变，`main.cpp`
的加载路径不用动。两套字符集，理由不同：

* `charset-ui.txt` —— 源码里出现过的界面字符（约 530 个）。标题和大数字用的 600 / 200 字重
  只装这一套就够。
* `charset-chat.txt` —— 界面的字 **加上 GB2312 全集**（6763 汉字 + 682 符号）。因为聊天面板
  显示的是**模型生成的中文**，任意汉字都可能出现；只装界面那几百个字，模型一开口还是 `????`。
  这是汉化第一版翻车的地方，正文那个字重（regular = `manrope-500`）必须装全集。

重新烘焙（需要 Python 的 fontTools 和一个主机 C++ 编译器）：

```sh
python3 -m pip install fonttools
# 字体是可变字体，必须是 TrueType 轮廓（00 01 00 00）：stb_truetype 不认 CFF/OTF
curl -L -o .deps/fonts/NotoSansSC-VF.ttf \
  'https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf'
scripts/bake-fonts.sh                     # = python3 scripts/bake-fonts-cn.py
```

脚本会：编译 `scripts/font-baker/bake_font.cpp`（改了上游那份，第 7 个参数支持
`@字符集文件`）→ 跑 `scripts/gen-charset.py` 生成两套字符集 → 用 fontTools 从可变字体切出
200/500/600 三个静态字重 → 烘焙三个 `.huifont` → **读回来核对每个字符都在**，缺字就报错退出。
产物直接进 git，控制台侧不需要字体工具链。

烘焙参数（`bake-fonts-cn.py` 顶部的 `BAKES`）：

| 文件 | 字重 | 像素 | SDF 范围 | atlas | 字符集 |
|---|---|---|---|---|---|
| `manrope-500.huifont` | 500（正文、聊天） | 36 | 6 | 4096² | UI + GB2312 |
| `manrope-600.huifont` | 600（标题、表格状态） | 64 | 8 | 4096² | UI |
| `manrope-200.huifont` | 200（大数字） | 64 | 8 | 4096² | UI |

6763 个汉字在 36px 下约 15.6M 像素，正好塞满 4096²；换成 64px 就放不下了。

下面这张图不是截图，是把 `manrope-500.huifont` 的图集直接按字形矩形和 advance 拼出来的
（`scripts/render-font-sample.py` 做的事）：能读出来，就说明这些字真的烘进字体里了。

![汉化字体样本](media/localization-font.png)

## 词条表与替换脚本

* `scripts/translations-zh.csv` —— 382 条，列是 `file,en,zh`。要改某个词的译法，改这里。
* `scripts/apply-translations.py` —— 只替换**带双引号的完整字面量**（`"Load"` 不会误伤
  `"Loading"`），源码里用 `\xNN` 转义写的符号也认得；有漏项时退出码为 1。原文件留一份
  `*.en.bak`（已在 `.gitignore` 里）。

在干净的源码树上一遍跑完：

```sh
python3 scripts/apply-translations.py .        # 438 处
```

译法规则：`printf` 占位符（`%s` `%.1f` `%zu` `%%`）原样保留、顺序不变；拼接处的首尾空格保留；
JSON 键名、`auto`/`none`、`.gguf`、量化名、仓库 id、API 路径、守护进程名、系统调用名一律不译；
标点用中文全角，中英文之间留一个空格。

## 改完怎么自查

```sh
python3 scripts/check-zh.py          # UTF-8、CRLF、引号成对、词条表干净（0 问题才算过）
python3 scripts/fitcheck-zh.py       # 每条译文的排版宽度和英文比，列出反而更宽的那些
python3 scripts/apply-translations.py . --dry-run
```

`fitcheck-zh.py` 用烘好的字体量字宽：界面是按固定位置排版的，译文比原文宽太多就会挤到邻居身上。
正常情况下列表很短（中文大多比英文窄）；出现宽度比接近或超过 1 的地方，最好把那条译短一点。

## 词条表覆盖不到的改动（手工改的代码）

这些是逻辑，不是文案，所以没进 CSV：

1. **`dashboard.cpp` 的 `wrap()`** —— 中文没有空格，一整段会是一个“词”，贪心断词断不开。
   加了一段：一行还是放不下就**按 UTF-8 字符**断行（每个多字节序列保持完整，否则最后一个字符
   会变成 U+FFFD）。聊天面板、存储对话框、调试报告对话框都靠它。
2. **`model_plan.hpp` 常量 `kDecisionPlanTag` / `kDecisionPlanSep`** —— 规划文案里
   `决策模型（clef），64k 上下文…` 这个前缀，在「模型」页要剥掉（那一行已经写了「决策」）。
   两处共用一个常量，免得哪天改了格式对不上。
3. **`activity.cpp`** —— 英文的 `" question" / " questions"` 单复数分支翻成中文后没意义了，
   改成 `" 个问题"`、`"，" + N + " 张图"`、`"："`。
4. **`main.cpp` 里 `text + 7`** —— 往日志里写时跳过 `"PS5LM: "` 前缀。中文冒号让前缀变成
   8 字节，改成 `text + std::strlen("PS5LM：")`。
5. **`dashboard.cpp` 下载行的分隔符** —— `", "` 换成 `"，"`（三处），以及「可放入 X。」的句号。
6. **硬盘名比较** —— `dashboard.cpp` 里有 `d.name == "应用存储"` 和
   `m.drive != "内置存储"`，必须和 `stats.cpp` / `storage.cpp` 里的名字一字不差。
7. **`model_plan_test.cpp`** —— 断言跟着改成用上面那两个常量拼。

## 文件清单

新增：

```
scripts/translations-zh.csv        词条表
scripts/apply-translations.py      替换脚本
scripts/gen-charset.py             从源码提取字符集 + GB2312
scripts/bake-fonts-cn.py           烘焙三个字重（跨平台，Python）
scripts/check-zh.py                编码/换行/引号/词条表自查
scripts/fitcheck-zh.py             译文排版宽度对比
scripts/render-font-sample.py      把 .huifont 里的字拼成 PNG（验证字形）
docs/media/localization-font.png   上面那个脚本对 manrope-500 的输出
scripts/font-baker/bake_font.cpp   改了上游那个烘焙器：支持 @字符集文件
scripts/font-baker/font_format.hpp .huifont 的二进制布局
docs/LOCALIZATION.md               本文件
ps5/app/assets/fonts/noto-sans-sc-LICENSE.txt
```

改动：

```
scripts/bake-fonts.sh              变成 bake-fonts-cn.py 的壳（上游那份 Manrope 做法留在注释里）
README.md                          中文
ps5/app/*.cpp, *.hpp               见上表
ps5/app/assets/fonts/*.huifont     重新烘焙（Noto Sans SC）
ps5/webui/index.html               浏览器聊天页
ps5/ps5lm/library.html             模型库页
.gitignore                         *.en.bak、.deps/fonts/
```

## 已知限制

* **主机上的键盘只打得出拉丁字母。** 手柄键盘（`ui::KeyboardLayout::letters()`）没有拼音输入，
  所以「△ 键发自己的话」只能打英文/数字；要看中文，用「□ 键发示例」或者浏览器里那个聊天页
  （那边是你自己的手机/电脑键盘）。这是上游界面的限制，不是汉化造成的。
* **截图和 GIF 还是英文的。** `docs/media/` 里是上游截的图；要在中文界面上重截，得在 Linux 上跑
  `scripts/preview-app-ui.sh`（需要 Mesa 的 surfaceless EGL）。
* **日志面板的中文字体是 DejaVu Sans Mono**（来自 ps5-homebrew-ui），里面没有汉字。所以日志行、
  API 页的 curl 示例、`llama-server` 参数都保持英文——它们本来就该是英文。
* **`docs/` 其余文档、`site/`、`fleet/control.html` 没有翻译**，它们面向的是构建、移植和集群
  调试，翻成中文反而不好和上游对照。
* **`ps5/ps5lm/app.cpp`（payload 那一侧）的消息**保持英文：它是 HTTP 响应和 llama-server 的
  包装层，改了会让客户端和 issue 里的报错对不上。

## 构建

本仓库是按 tar 包起的 git 仓库，`third_party/*` 里的三个 submodule 没有 gitlink 记录，所以
`git submodule update --init` 不会自己动。构建前按 `.gitmodules` 里的地址把它们放到对应目录：

```sh
git clone https://github.com/Phononenom/ps5-homebrew-ui third_party/ps5-homebrew-ui
git clone https://github.com/ggml-org/llama.cpp           third_party/llama.cpp
git clone https://github.com/drakmor/ftpsrv               third_party/ftpsrv
```

之后按 [BUILDING.md](BUILDING.md) 的步骤来（需要 PS5 的 payload SDK 和 vulkan 头）。字体和页面
的构建产物已经在仓库里，控制台侧不需要 fontTools，也不需要重新生成页面。

## 和上游同步

```sh
git remote add upstream https://github.com/cobanov/PS5LM.git
git fetch upstream && git merge upstream/main
python3 scripts/apply-translations.py . --dry-run   # 看新文案还差哪些词条
```

`--dry-run` 列出所有没命中的词条：新版本新增的英文会出现在这里，补进 CSV 再跑一遍真替换，
最后 `scripts/bake-fonts.sh` 重烘字体（新文案里可能出现新的汉字）。
