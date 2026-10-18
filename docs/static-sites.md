# 原生静态站点

QSNote 使用 C++ 和内置 Qt WebEngine 生成静态站点，创建、预览和生成均可离线完成，不需要 Hexo、Node.js、npm 或额外下载。Pure 和 NexT（Muse） 保持原主题的页面布局、导航和配色，文章正文使用编辑器预览的默认样式，可在「站点配置 → 主题与页脚」切换。C++ 按原模板的 DOM 结构输出 HTML，直接使用原主题的 CSS、字体、图片和浏览器脚本；NexT 的 Stylus 已预编译为随程序分发的静态 CSS。构建和生成不读取 EJS、Nunjucks、Stylus，不执行 Hexo 插件。旧配置中的 `stellar` 在读取或保存时自动迁移为 `next`，重新生成会清除旧主题输出资源。

## 文件结构

```text
站点/
├── _index.json          # QSNote 索引、站点与发布设置
├── source/
│   ├── _posts/          # 文章，按分类存放
│   ├── _drafts/         # 草稿，不发布
│   ├── categories/index.md
│   ├── tags/index.md
│   ├── links/index.md
│   └── about/index.md
└── public/              # 完整生成结果，可部署到静态服务器
```

站点信息、主题、页脚、友链和评论在配置界面维护。生成器只将页面需要的设置写入 HTML，发布凭据不会复制到输出。不要在 `public` 中直接修改文件；每次成功生成都会替换整个目录。需要发布的图片、下载附件和 `CNAME` 等文件放在 `source` 中，自定义页面使用 Markdown，例如 `source/guide/index.md` 生成 `guide/index.html`。已有页面不会在保存时被覆盖。

站点配置通过顶部 Tab 切换基本信息、评论、主题与页脚、分类管理、友链、生成与发布，每次只显示一张卡片，切换时保留已填写内容。分类管理支持新建、重命名和删除；重命名同步更新文章和草稿的分类，删除分类会将其中的文章和草稿移入 `default`，保留正文。内置 `default` 分类不可重命名或删除。

## 文章和页面

保存文章时，QSNote 写入带 `qsnote: true` 标记的 JSON front matter，包含标题、日期、slug、分类、标签。应用管理的 front matter 不显示在正文编辑区，修改文章属性会同步回文件。也可以在正文开头自行编写 front matter：支持 JSON 对象，以及简单 YAML 的标量、字符串列表和布尔值；不支持 YAML 嵌套对象、锚点或多行标量。格式不支持时会报告文件名和错误。

```markdown
---
title: 示例文章
date: 2026-10-12
slug: example
categories: [开发]
tags:
  - C++
  - 笔记
comments: false
---
# 正文
```

`slug` / `permalink` 是相对站点根目录的路径，默认生成目录下的 `index.html`，也支持 `.html` 路径。重复输出路径（包括与首页、归档、分类页等冲突）和包含 `..` 的路径会导致生成失败。`published: false`、`draft: true` 和 `_drafts` 下的文章均不发布。

正文与编辑器预览共用 Markdown-it、Prism 和 CSS，支持标题、列表、引用、表格、删除线、脚注、上下标、标记、提示块和代码高亮。原始 HTML 使用与预览相同的安全过滤规则。公式及 Mermaid、Flowchart、Graphviz、WaveDrom、PlantUML 图表在生成时固化为图片，阅读时无需渲染脚本或网络；PlantUML 需要本地引擎。渲染失败会报告文章并保留上次生成结果。个性化阅读设置仅影响编辑器。文章页保留分类、标签和上下篇链接，NexT 使用原来的侧栏标题目录。站内 Markdown 链接及本地图片会按输出路径重写；`_posts` 内的图片和附件输出到 `assets/posts`。原始 HTML 中的图片和链接也使用相同的路径映射。

文章按日期降序排列，列表和归档每页 10 篇。搜索沿用主题原有界面：Pure 的弹层读取本地 `content.json`，NexT 的搜索弹层读取 `search.json`，均支持标题和正文。Pure 保留原来的作者导航栏、内容区、右侧组件和底部翻页栏；NexT 保留 Muse 的顶部导航、文章列表、标题区和可切换的目录侧栏。响应式断点、字体、间距和颜色沿用原主题 CSS。

生成先写临时目录，完成后替换 `public` 并清除过期页面；解析、路径冲突或写入失败时保留上一次输出。

## 预览和发布

预览使用只监听本机回环地址的临时 HTTP 服务和 Qt WebEngine，关闭预览即停止服务。域名支持子路径，例如 `https://example.com/blog/`，预览和输出中的站点链接使用相同的 `/blog/` 前缀。

评论默认关闭，可选择 Valine、Gitalk 或 Disqus；仅文章页加载所选服务，页面可以用 `comments: false` 单独关闭。第三方评论脚本、图片放大等原主题的可选组件需要联网，其公开客户端配置会嵌入文章页。生成站点本身不联网。

在「站点配置 → 生成与发布 → 发布方式」中选择 **GitHub Pages** 或 **SSH SCP**，只显示对应的设置项；切换方式保留已填写设置。新建和旧站点默认未选择发布方式，不影响预览。点击「发布站点」后，应用先校验设置，再生成并上传完整 `public` 内容（不含草稿、应用索引、依赖和发布配置）。

- **GitHub Pages**：填写仓库 SSH/HTTPS 地址和发布分支（留空为 `gh-pages`）。需要 PATH 中的 `git`，认证使用本机 Git 凭据或 SSH 密钥。首次发布创建分支，后续发布保留提交历史、替换该分支根目录的内容并正常推送；推送冲突或认证失败会提示失败。应用自动添加 `.nojekyll`。在 GitHub 仓库 **Settings → Pages → Deploy from a branch** 选择发布分支和 `/ (root)`。推送完成后仍需等待 GitHub 部署；应用不自动修改仓库 Pages 设置。站点「域名」填写实际 URL，项目站点须包含仓库子路径，例如 `https://user.github.io/repo/`。自定义域名需要的 `CNAME` 放在 `source/CNAME`。参见 [GitHub Pages 发布来源设置](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)。
- **SSH SCP**：填写主机（域名、IP 或 SSH 配置别名）、端口（留空为 `22`）、用户名、远程绝对目录和可选私钥文件完整路径。远程目录支持文字、数字、下划线、短横线、点和斜杠，不含空格或 shell 特殊字符。需要 PATH 中的 `ssh`、`scp`，通过密钥或 ssh-agent 认证；先在终端连接主机并确认指纹。发布时创建目标目录，上传 `public` 的内容（含隐藏文件），覆盖同名文件，保留远端其他文件；删除的文章不会自动从服务器删除。参见 [OpenSSH scp 手册](https://man.openbsd.org/scp)。

发布采用非交互认证，不在应用内收集或保存密码、Token 或私钥内容。设置保存在站点 `_index.json` 中；SSH 私钥字段只保存路径。每条发布命令最多等待 5 分钟，失败会显示命令错误；上传过程中失败可能留下部分已更新的文件，修复连接或权限后可重新发布。

## 验证

```sh
cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`qsnote_site` 在清空 PATH 后验证 Pure/NexT 生成，并覆盖文章与元数据、分页、分类标签、归档、搜索、评论、资源和站内链接、中文子路径、失败保留输出和过期页面清理。`qsnote_publish` 验证本地 Git 发布及模拟 SSH/SCP 参数；`qsnote_ui` 验证两套主题的 WebEngine 预览、实际搜索交互、手机菜单开合、配置持久化及发布入口。主题 CSS 的 SHA-256 校验可防止原有布局和配色被意外改动。资源来源见 `src/resources/site/README.md`。
