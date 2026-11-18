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

1. `scripts/build-app.sh` 把 `ps5/webui/index.html` 和 `app.json` 拷进包的 `assets/www/`。
2. `Models::start`（`ps5/app/main.cpp`）在启动 llama-server 前补一个参数：
   `--path <app0>/assets/www`（`app0` 走 `sandbox_path()`，HEN 把 app 挪出沙箱时也对）。
   llama-server 会把 `index.html` 挂在 `/` 上，并且因为 `public_path` 非空，**不再**serve 自带的 UI
   （顺带清掉浏览器里那套旧的 service worker）。
3. 页面只用两个接口：`GET /v1/models`（顶栏显示模型名）和 `POST /v1/chat/completions`
   （`stream: true`，按 SSE 增量显示，带 token 数和 token/秒）。纯 ES5 + XMLHttpRequest，
   没有外部文件和打包步骤，因为主机浏览器认不了现代 bundle。

## 换回 llama.cpp 的页面

`/data/PS5LM/env.txt` 里写一行：

```
PS5LM_WEBUI=llama
```

重开 app（或换模型）后生效。适合在 PC 浏览器上看那套完整 UI；主机浏览器上多半还是空白。

优先级：`app-args.txt` 里自己带的 `--path` > `PS5LM_WEBUI` > app 自带的页面。

## 页面上的「模型」链接

payload 那条路（`ps5lm.elf`）在 8082 端口有一页模型库，所以链接指过去。native app 没有这一页
（模型库在主机电视上），它在 `assets/www/app.json` 里写了 `{"library": ""}`，页面读到就把链接
藏起来；payload 的 www 目录里没有 `app.json`，链接照旧。

## 怎么验证

- 主机上加载一个模型，主页显示「就绪」后，PC 浏览器打开 `http://<主机>:8081`：应该是黑底、
  顶栏 `ps5lm` + 模型名、中间「问点什么」和一排示例问题、底部输入框（占位符「说点什么」、按钮「发送」）。
- app.log 里会有一行 `the chat page is the app's own (/app0/assets/www)`；用 `PS5LM_WEBUI=llama`
  时这行不出现。
- 回复是逐字出来的（SSE）；停下按钮会 abort 当前请求。
- CI 会检查包里有没有 `assets/www/index.html`、它是不是中文那一页。

## 没验证的部分

- 页面在 **PS5 自带浏览器**里的样子没有在真机上看过（上一版连页面都没进包，这一版才进）。
- 只验证了它在 PC 浏览器里能拿 `/v1/models` 和流式 `/v1/chat/completions`；主机浏览器的 XHR
  行为、字体（页面用系统 SST，缺字回落到 Segoe/Roboto）要真机确认。
- 会话不落盘：刷新页面即清空，最多带最近 12 条消息。
