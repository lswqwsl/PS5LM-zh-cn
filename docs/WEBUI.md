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
3. llama-server 会把 `index.html` 挂在 `/` 上，并且因为 `public_path` 非空，**不再**serve 自带的
   UI（顺带清掉浏览器里那套旧的 service worker）。
4. 页面只用两个接口：`GET /v1/models`（顶栏显示模型名）和 `POST /v1/chat/completions`
   （`stream: true`，按 SSE 增量显示，带 token 数和 token/秒）。纯 ES5 + XMLHttpRequest，
   没有外部文件和打包步骤，因为主机浏览器认不了现代 bundle。

### 必须记住的坑：`--path` 指向 `/app0` 会让 llama-server 直接不启动

第一版是直接传 `--path /app0/assets/www`。app 里的 `access()` 说这个目录在（字体也是从
`/app0/assets/fonts` 读的），但 llama.cpp 用 `stat()` 打开 `--path`，而 title 自己的 `/app0`
虚拟根对它**不是**一个能 stat 出来的目录。结果：

```
E srv init_listene: static assets path not found: /app0/assets/www
E srv llama_server: failed to initialize HTTP server
```

llama-server 在加载模型之前就退出了 —— 日志里 `llama_server returned 1`，端口 8081 从没打开，
电视上显示「服务已停止」，浏览器自然连不上。所以页面文件**永远不要**放在 `--path` 上赌；
现在是先复制到 `/data`（模型就是从那里读的，肯定能 stat），再核对一遍。

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
- app.log 里会有一行 `the chat page is the app's own (/data/PS5LM/www)`；用 `PS5LM_WEBUI=llama`
  时这行不出现。
- 回复是逐字出来的（SSE）；停下按钮会 abort 当前请求。
- CI 会检查包里有没有 `assets/www/index.html`、它是不是中文那一页。

## 没验证的部分

- 页面在 **PS5 自带浏览器**里的样子没有在真机上看过（上一版连页面都没进包，这一版才进）。
- 只验证了它在 PC 浏览器里能拿 `/v1/models` 和流式 `/v1/chat/completions`；主机浏览器的 XHR
  行为、字体（页面用系统 SST，缺字回落到 Segoe/Roboto）要真机确认。
- 会话不落盘：刷新页面即清空，最多带最近 12 条消息。
