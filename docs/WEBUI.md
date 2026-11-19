# 网页端：`http://<主机>:8081` 上的那一页

浏览器打开 `http://<主机>:8081` 看到的就是 **llama.cpp 自带的那套 web UI**
（`third_party/llama.cpp/tools/ui`），只是被汉化了 —— 不是另写一页。

## 为什么是它，而不是自己写一页

上游 PS5LM 的包里就有这套 UI：从它的 `eboot.bin` 里能直接抽出 `index.html`、
`manifest.webmanifest`、`sw.js`、`build.json`（`{"version":"b12032"}`）和那一整套图标，
说明它把 llama.cpp 的 UI 编进了 `llama-server`。用自己手写的页面替换它，布局、模型库、
设置、MCP、附件预览全都没了 —— 那不是汉化，是换了个东西。所以这里改成：**同一套 UI，只换文字。**

## 现在是怎么接上的

1. `patches/0014-webui-chinese.patch` 把 `tools/ui/src` 里**用户能看到的文字**换成中文：
   模板里的文本节点、`placeholder`/`title`/`aria-label`/`alt`、以及模块里
   `label`/`title`/`description` 这类字符串，共 491 条（610 处），另外 `app.html` 的
   `lang` 改成 `zh-CN`。只动引号里的内容，不碰任何代码结构。
2. `scripts/build-app.sh` 用 `-DLLAMA_BUILD_UI=ON -DLLAMA_USE_PREBUILT_UI=OFF` 配置 CMake：
   它会跑 `tools/ui` 的 `npm ci && npm run build`，把构建出的 `dist/` 逐个 gzip 后
   **嵌进 `llama-server`**（生成的 `ui.cpp` 里是一张表 + 每文件一份字节数组）。
   所以包里没有 `assets/www` 目录，页面在 eboot 里。
3. 运行时 llama-server 直接把这些资源挂在 `/` 上，页面和 API 同源，不需要 `--path`、
   不需要 CORS，也没有"页面文件放哪"的问题。
4. CI 会检查 `eboot.bin` 里确实有 `manifest.webmanifest`、`index.html`，而且有 `说点什么`
   和 `新聊天` —— 少一个就判定这次没有 UI（CMake 在拿不到资源时会安静地产出一个没有页面的
   server，不报错）。
5. 页面自己走 `/v1/models`、`/props`、`/v1/chat/completions`，模型加载好之前它自己会显示
   "加载中"，不用我们再写状态机。

## 需要 node/npm

`LLAMA_BUILD_UI=ON` 要 node/npm：CI 在容器里 `apt-get install nodejs npm`（trixie 自带
node 20），本地构建同理。`scripts/build-app.sh` 找不到 `npm` 会直接报错退出，
而不是产出一个没有页面的包。想跳过这件事：`PS5LM_BUILD_UI=OFF scripts/build-app.sh`。

## 翻译是怎么做的、怎么改

- 清单：`tools/ui/src` 里模板文本 + 那些属性/字段，共 285 + 141 + 34 + 66 条候选（模板文本、属性、字段、UPPER_SNAKE 标签常量、设置页的 help 文案），其中 491 条翻成中文、
  15 条故意跳过（Svelte 表达式片段、`llama-server -hf ...` 这种命令行示例、以及纯内部的
  迁移日志说明）。
- 术语：conversation 会话、chat 聊天、model 模型、server 服务器、context 上下文、
  attachment 附件、tool 工具、resource 资源、system message 系统消息、KV cache KV 缓存。
- 要改哪句：直接在 `tools/ui/src/...` 里改中文，然后重新生成补丁（或者改补丁本身）。
  注意补丁是打在 llama.cpp 子模块上的，`git -C third_party/llama.cpp apply` 打，改完提交
  新的 `patches/0014-webui-chinese.patch`。
- 想回到英文原版：构建时 `PS5LM_BUILD_UI=ON` 但不打 `0014` 补丁；或者运行时不带 UI：
  在 `app-args.txt` 里加一行 `--no-ui`（llama-server 只提供 API）。

## 和上游不一样的地方

- 上游的包里 UI 是英文，这里是中文；按键、布局、页面都一样。
- `patches/0013-webui-without-realpath.patch` 保留着：`--path` 在主机上挂不上时
  （llama.cpp 的 `set_mount_point()` 除了 `stat` 还要 `realpath`）不再让整个 server 退出，
  而是用 stdio 把目录里的页面文件回出去。默认构建不走这条路，但 `app-args.txt` 里写
  `--path` 的人会用到它。

## 怎么验证

- app.log 里有一行 app 自己发出去的 `GET /`：
  `chat page: GET / -> 200 OK, 12345 bytes` —— 200 加字节数就是页面在；
  `404` 说明 server 起来了但没有页面（模型照常可用）；`no answer` 说明它没监听上。
- PC 浏览器打开 `http://<主机>:8081`：左侧会话列表、中间输入框、右上设置，界面文字是中文。
- 主机自带浏览器大概率还是空白页（那台浏览器的内核太老，上游也是这个结论）；
  这一页是给 PC 和手机浏览器用的。

## 没验证的部分

- 汉化后的 UI 还没在真机的浏览器里看过一遍（谁要是发现某句还是英文，告诉我位置就行）。
- 页面里的长尾功能（MCP、附件预览、模型库发现页）只翻了界面文字，没逐个点过。
