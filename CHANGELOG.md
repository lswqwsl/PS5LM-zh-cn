# 变更记录

这个 fork 基于 [cobanov/PS5LM](https://github.com/cobanov/PS5LM) **v0.11.0**
（2026-11-02）。版本号、`contentVersion`（`01.011.000`）与上游保持一致，只改界面语言、字体和
网页端文案。上游自己的变更见其仓库的 CHANGELOG。

## [0.11.0]

发布页只有这一个包，`ps5lm-app-v0.11.0.zip`；早期发过的 0.11.1 / 0.11.2 是走过的弯路，
已删除。

**网页端**

- `:8081` 是 llama.cpp 自带的 web UI，**882 条界面文案换成中文**
  （`patches/0014-webui-chinese.patch`，覆盖模板文本、属性、设置页说明、按钮与提示语、
  错误信息），`app.html` 的 `lang` 改为 `zh-CN`。
- 构建用 `-DLLAMA_BUILD_UI=ON -DLLAMA_USE_PREBUILT_UI=OFF`：CMake 跑 `tools/ui` 的 npm 构建，
  把 `dist/` gzip 后嵌进 `llama-server`，和上游的打包方式一致；包里没有 `assets/www`。
- CI 会解开 `eboot.bin` 检查 UI 真的在里面且是中文（CMake 拿不到资源时会安静地产出没有页面的
  server，不报错）。
- `patches/0013-webui-without-realpath.patch`：llama.cpp 的 `set_mount_point()` 除了 `stat()`
  还要 `realpath()`，主机上前者过、后者不过，`--path` 会让 server 在加载模型前退出；现在改成
  用 stdio 直接回文件，页面上不去也不会拖垮 server。
- app 自己会发一次 `GET /` 并记进 app.log（`chat page: GET / -> 200 OK, 12642 bytes`），
  不用开浏览器就知道页面在不在。

**电视端**

- 五个页面、提示行、对话框、通知、模型库/下载状态、推荐列表介绍全部中文。
- 正文字体重烘为 Noto Sans SC 的 GB2312 全集（7552 字形），模型生成的中文不再显示成 `????`。
- 启动画面从第一帧就读手柄：以前 pad 在启动线程之后才打开，读模型头信息那十来秒里按什么都没反应。
- 文档整理：README 重写，删掉上游的研究笔记、路线图、移植笔记和体积较大的英文截图。

**构建**

- CI 容器里装 node/npm 构建网页 UI；`scripts/build-app.sh` 找不到 npm 会直接报错，而不是产出
  一个没有页面的包。
- 上游的 zlib 镜像偶尔给截断的档（`sha256sum` 报 `1 computed checksum did NOT match`），
  构建前自己按 sha256 取一份，坏就换 `madler/zlib` 的 release。

## 发布历史

| 版本 | 说明 |
|---|---|
| v0.11.0 | 就是上游那一版，加了汉化。包的资产随修 bug 更新过，发布页只留最新这一份 |
