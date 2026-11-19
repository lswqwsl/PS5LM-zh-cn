# 网页端：`http://<主机>:8081` 上的那一页

app 里的 llama-server 自己会往外提供一个页面，浏览器打开 `http://<主机>:8081` 看到的就是它。
这一版起，那是 **app 自带的汉化聊天页**（`ps5/webui/index.html`，打包进
`assets/www/index.html`），不再是 llama.cpp 自带的那一套。

## 为什么要换

- llama.cpp 自带的 UI 是 React/SvelteKit 打包出来的一整包 JS，**只有英文**；上游自己的注释也说
  它在 PS5 浏览器里渲染成空白页，所以 `scripts/ps5-chat.sh`（payload 那条路）一直给 llama-server
  传 `--path www`，用仓库里这个手写页面代替它。
- 但 native app 这条路一直没有传 `--path`，于是网页端落回 llama.cpp 的英文 UI——也就是「网页端
  完全没有翻译」的原因。这一版补上了。

## 现在是怎么接上的

1. `scripts/build-app.sh` 把 `ps5/webui/index.html` 和 `ps5/webui/app.json` 拷进包的
   `assets/www/`。
2. 启动 llama-server 前，`install_chat_page()`（`ps5/app/main.cpp`）把这两份文件**复制到数据目录**
   `/data/PS5LM/www/`（`/data` 没开时是 `/download0/PS5LM/www/`），然后用 `stat` + `opendir`
   核对它确实是个目录、`index.html` 确实是个非空文件，才给 llama-server 补
   `--path <那个目录>`。
3. llama-server 把那个目录挂到 `/` 上（`public_path` 非空，自带的英文 UI 也就不再 serve）。
   挂不上也不再致命：`patches/0013-webui-without-realpath.patch` 在 `set_mount_point()` 返回
   false 之后，改用 stdio 把同一个目录里的 `index.html` / `app.json` 直接回给浏览器，
   **而且挂不上只是少一个页面，绝不会再把整个 server 带走**。
4. 页面只用两个接口：`GET /v1/models`（顶栏显示模型名）和 `POST /v1/chat/completions`
   （`stream: true`，按 SSE 增量显示，带 token 数和 token/秒）。纯 ES5 + XMLHttpRequest，
   没有外部文件和打包步骤，因为主机浏览器认不了现代 bundle。

### 必须记住的坑：`--path` 挂不挂得上，卡的是 `realpath()`，不是 `stat()`

第一版直接传 `--path /app0/assets/www`，llama-server 打印：

```
E srv init_listene: static assets path not found: /app0/assets/www
E srv llama_server: failed to initialize HTTP server
```

第二版把页面复制到 `/data/PS5LM/www`、并且先用 `stat` + `opendir` 核对过，报的还是同一句
（`/data/PS5LM/www`）。真正的原因在 httplib 的 `set_mount_point()`：

```cpp
detail::FileStat stat(dir);
if (stat.is_dir()) {
    std::string resolved_base;
    if (!detail::canonicalize_path(dir.c_str(), resolved_base)) {   // realpath(3)
        return false;
    }
```

它先 `stat` 再 `realpath`：**`stat` 这一关过了，`realpath` 过不去照样返回 false**，而主机上
`realpath()` 对这种数据目录就是失败（app 自己的 `stat` / `opendir` 都正常）。所以「目录在不在」
不能只用 `stat` 判断——这条路上真正被卡住的是 `realpath`。

后果比页面本身严重得多：`return false` 让 `init_listener` 失败 → `llama_server returned 1`，
模型**根本没开始加载**，端口 8081 从没打开，电视上显示「服务已停止」。所以 `patches/0013` 把
「挂不上」降级成一条警告加 stdio 路由，页面和模型各活各的。

## 换回 llama.cpp 的页面

`/data/PS5LM/env.txt` 里写一行：

```
PS5LM_WEBUI=llama
```

重开 app（或换模型）后生效。适合在 PC 浏览器上看那套完整 UI；主机浏览器上多半还是空白。

优先级：`app-args.txt` 里自己带的 `--path` > `PS5LM_WEBUI` > app 自带的页面。

`PS5LM_WEBUI=off` 则完全不传 `--path`（llama-server 也就没有页面，只有 API）。

## 页面上的「模型」链接

payload 那条路（`ps5lm.elf`）在 8082 端口有一页模型库，所以链接指过去。native app 没有这一页
（模型库在主机电视上），它在 `ps5/webui/app.json`（包里是 `assets/www/app.json`）里写了
`{"library": ""}`，页面读到就把链接藏起来；payload 的 www 目录里没有 `app.json`，链接照旧。

## 怎么验证

- 主机上加载一个模型，主页显示「就绪」后，PC 浏览器打开 `http://<主机>:8081`：应该是黑底、
  顶栏 `ps5lm` + 模型名、中间「问点什么」和一排示例问题、底部输入框（占位符「说点什么」、按钮「发送」）。
- app.log 里先有 `the chat page is the app's own (/data/PS5LM/www)`（用 `PS5LM_WEBUI=llama`
  时这行不出现），接着有一行 app 自己发出去的 GET：
  `chat page: GET / -> 200 OK, 14300 bytes`。这一行是**真的去问了一次**：`200` 说明页面在，
  `404` 说明 server 起来了但没页面，`no answer` 说明它压根没监听上。
- llama.log 里正常应该只有 `PS5LM: serving the chat page from /data/PS5LM/www`，
  前面带一条 `PS5LM: no mount point for …: stat() = 0 (errno 0, a directory)`——
  后面这句正是「`stat` 说它在、`realpath` 说不行」，有了它就不再靠猜。
- 回复是逐字出来的（SSE）；停下按钮会 abort 当前请求。
- CI 会检查包里有没有 `assets/www/index.html`、它是不是中文那一页。

## 没验证的部分

- 页面在 **PS5 自带浏览器**里的样子没有在真机上看过（上一版连页面都没进包，这一版才进）。
- 只验证了它在 PC 浏览器里能拿 `/v1/models` 和流式 `/v1/chat/completions`；主机浏览器的 XHR
  行为、字体（页面用系统 SST，缺字回落到 Segoe/Roboto）要真机确认。
- 会话不落盘：刷新页面即清空，最多带最近 12 条消息。
