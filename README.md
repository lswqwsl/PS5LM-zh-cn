# PS5LM 简体中文版

在越狱的 PS5 上跑 llama.cpp：电视上一个中文界面，浏览器里一套中文聊天页，模型全部放在主机的
GPU 内存里。这是 [cobanov/PS5LM](https://github.com/cobanov/PS5LM) **v0.11.0** 的汉化 fork，
版本号与上游一致，差别只在界面语言、字体和网页端文案。

## 安装

1. 从 [Releases](https://github.com/lswqwsl/PS5LM-zh-cn/releases/latest) 下载 `ps5lm-app-v0.11.0.zip` 并解压，得到 `PPSA99575` 文件夹。
2. 用 FTP 把 `PPSA99575` 覆盖到主机的 `/data/homebrew`（ShadowMountPlus 会把它加到主屏幕）。
3. 打开 PS5LM，进 **模型 > 推荐** 拿一个模型，或者把 `.gguf` 放到 `/data/PS5LM/models`。
4. 加载它。主页显示「就绪」后，浏览器打开 `http://<主机>:8081` 就能聊。

需要已破解的 PS5 和 kstuff-lite / etaHEN 这类自制环境。设置、模型和下载都在 `/data/PS5LM`，
覆盖安装不会动它们。写不了 `/data` 时（HEN 没把存储开给 app），参见
[docs/CONSOLE.md](docs/CONSOLE.md)。

## 和上游有什么不同

- **电视端全中文**：五个页面、提示行、对话框、通知、模型库与下载状态、推荐列表介绍。
- **网页端是原版那一页，只是中文**：`http://<主机>:8081` 是 llama.cpp 自带的 web UI（会话列表、
  模型选择、设置、MCP、附件预览都在），882 条界面文案换成了中文，构建时用 npm 编译进
  `llama-server`。见 [docs/WEBUI.md](docs/WEBUI.md)。
- **中文字体重烘**：正文图集是 Noto Sans SC 的 GB2312 全集（7552 字形），模型生成的中文不再
  显示成 `????`。
- **手柄开机能用**：启动画面从第一帧就读手柄（以前要先等模型列表规划完，几秒内按什么都没反应）。
- 其余功能、性能、参数与上游一致；OpenAI / Anthropic 客户端走 `/v1/...`，不受影响。

## 从源码构建

需要一个 Linux 环境（CI 用 Debian trixie 容器）和 Node/npm（构建网页 UI 用）：

```sh
scripts/setup-sdk.sh          # 一次即可：拉 payload SDK 与工具链
scripts/build-app.sh          # 交叉编译 + 打包成 build/release/ps5lm-app-v*.zip
```

细节：[docs/BUILDING.md](docs/BUILDING.md)、[docs/CI.md](docs/CI.md)。
改了界面文案要重烘字体：[docs/LOCALIZATION.md](docs/LOCALIZATION.md)。

## 文档

| 文件 | 内容 |
|---|---|
| [docs/WEBUI.md](docs/WEBUI.md) | `:8081` 那一页是怎么汉化、怎么构建进去的 |
| [docs/LOCALIZATION.md](docs/LOCALIZATION.md) | 汉化了哪些地方、字体怎么烘、怎么自查 |
| [docs/CONSOLE.md](docs/CONSOLE.md) | 主机上的实测数据与坑（存储、手柄、性能） |
| [docs/CLI.md](docs/CLI.md) | app 认的命令行参数与 `app-args.txt` |
| [docs/MODELS.md](docs/MODELS.md) | 模型格式、量化与上下文规划 |
| [docs/FLEET.md](docs/FLEET.md) | 实验性的多主机集群 |
| [docs/INTROSPECTION.md](docs/INTROSPECTION.md) | 看模型内部（logits / 激活）的接口 |
| [docs/BUILDING.md](docs/BUILDING.md) / [docs/CI.md](docs/CI.md) | 构建与 CI |
| [CHANGELOG.md](CHANGELOG.md) | 这个 fork 改了什么 |

## 许可

GPL-3.0-or-later，与上游一致。字体：Noto Sans SC（OFL）、DejaVu Sans Mono（Bitstream Vera）、
Manrope（OFL，仅字重骨架）；界面声音是 PocketVibe 的；ps5-exporter 与 ftpsrv 的许可跟着各自的
NOTICE 一起进包。
