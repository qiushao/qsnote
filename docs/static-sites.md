# 原生静态站点

QSNote 使用 C++ 和内置 Qt WebEngine 生成静态站点，创建、预览和生成均可离线完成，不需要 Hexo、Node.js、npm 或额外下载。Pure、NexT（Muse）和 Yilia 保持原主题的页面布局、导航和配色，文章正文使用编辑器预览的默认样式，可在「站点配置 → 主题与页脚」切换。C++ 按原模板的 DOM 结构输出 HTML，使用原主题的 CSS、字体和图片，按主题接入浏览器脚本；NexT 的 Stylus 已预编译为随程序分发的静态 CSS，Yilia 使用上游预编译 CSS。构建和生成不读取 EJS、Nunjucks、Stylus，不执行 Hexo 插件。旧配置中的 `stellar` 在读取或保存时自动迁移为 `next`，重新生成会清除旧主题输出资源。

## 自定义主题与 Hexo 适配

在「站点配置 → 主题与页脚」选择 **自定义**，填写或选择主题目录。路径可以相对于站点根目录（例如 `themes/my-theme`），也可以是完整路径；选择站点内部目录时自动保存相对路径。切回内置主题会保留目录设置。主题直接从该目录读取，修改后重新预览即可生效。

可先将仓库中的 [`examples/site-theme`](../examples/site-theme) 复制到站点的 `themes/my-theme`，再选择它。这是可直接生成的最小示例。

兼容范围为 Hexo 的目录组织与已编译静态资源，**不是直接运行任意 Hexo 主题**。参见 [Hexo 主题结构](https://hexo.io/docs/themes)。QSNote 不读取主题 `_config.yml`、`languages/`，不执行 `scripts/`、插件、Hexo helper 或模板中的 JavaScript；标题、头像、页脚、评论等继续使用站点配置。迁移时将 EJS / Nunjucks / Pug / Swig 模板改写为下面的 HTML 占位符格式，将 Stylus / Sass / Less 预编译为 CSS，再放入主题 `source/`。生成和使用适配后的主题不需要 Node.js 或网络。

```text
themes/my-theme/
├── layout/
│   ├── layout.html     # 必需：完整 HTML 文档，{{body}} 为当前页面
│   ├── index.html      # 必需：文章列表，{{body}} 为本页列表项
│   ├── card.html       # 必需：单篇文章的列表项
│   ├── post.html       # 必需：文章正文页
│   ├── page.html       # 必需：普通页、分类/标签总览、友链页
│   ├── archive.html    # 可选：归档列表，缺省使用 index.html
│   ├── category.html   # 可选：某个分类，缺省使用 archive → index
│   └── tag.html        # 可选：某个标签，缺省使用 archive → index
└── source/
    ├── css/theme.css
    ├── js/theme.js
    └── images/...
```

`source/` 中的文件按原路径输出到 `public/`，忽略名称以 `.` 或 `_` 开头的文件及目录；可以省略该目录。浏览器 JS 原样复制，在浏览页面时执行。`.styl`、`.stylus`、`.scss`、`.sass`、`.less`、`.ejs`、`.njk`、`.nunjucks`、`.pug`、`.swig` 文件会提示预编译错误；构建工具源码应放在 `source/` 外。主题根目录可选的 `LICENSE`、`THIRD-PARTY-NOTICES.md` 分别输出为 `theme-LICENSE`、`theme-THIRD-PARTY-NOTICES.md`。主题资源与文章附件、生成页面等重名时报告冲突，不覆盖。

模板只支持 `{{变量}}` 和非嵌套条件块 `{{#变量}}内容{{/变量}}`。变量非空时保留条件块；不支持循环、表达式、include 和嵌套条件。文章循环、排序、每页 10 篇的分页由生成器完成。未知变量、格式错误、未闭合/嵌套条件块和缺失/空模板会报告文件路径，并保留原有 `public/`。

| 适用模板 | 变量 |
| --- | --- |
| 所有模板 | `root`、`siteTitle`、`author`、`city`、`githubSocial`、`avatar`、`avatarImage`、`description`、`footer`、`domain` |
| `layout` | `headTitle`、`title`、`body`、`permalink`、`keywords`、`articleStyle` |
| `index` / `archive` / `category` / `tag` | `title`、`body`、`pagination`、`pageNumber`、`pageCount`、`postCount` |
| `card` | `title`、`url`、`date`、`meta`、`categories`、`tags`、`excerpt` |
| `post` | `title`、`body`、`url`、`permalink`、`date`、`meta`、`categories`、`tags`、`navigation`、`comments`、`toc` |
| `page` | `title`、`body` |

文本和 URL 变量已进行 HTML 转义。`body`、`meta`、`categories`、`tags`、`navigation`、`comments`、`toc`、`pagination`、`avatarImage`、`articleStyle` 为生成器提供的 HTML，直接插入。变量值不会被当成模板再次执行；当前模板未提供的变量为空。没有配置头像时 `avatar` 与 `avatarImage` 为空。

在 `layout.html` 的 `<head>` 中放入 `{{articleStyle}}`，以加载应用的正文样式；`post.html`、`page.html` 使用 `{{body}}` 输出已渲染正文，无需再次解析 Markdown。在页面外框中引用资源使用 `{{root}}css/theme.css`，`root` 已包含域名子路径及末尾 `/`，例如 `/blog/`。CSS 中的相对图片路径按 CSS 所在位置解析。自定义主题自行提供导航、搜索界面和交互；生成器仍输出 `search.json` 与 `content.json`，评论通过文章模板的 `{{comments}}` 接入。

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

站点配置采用左侧垂直导航、右侧设置内容的布局，可切换基本信息、评论、主题与页脚、分类管理、友链、生成与发布、站长统计、自定义代码，每次只显示当前栏目的设置，右侧内容可独立滚动，切换时保留已填写内容。分类管理支持新建、重命名和删除；重命名同步更新文章和草稿的分类，删除分类会将其中的文章和草稿移入 `default`，保留正文。内置 `default` 分类不可重命名或删除。

「基本信息 → 头像」支持填写图片网址或 `source` 内的相对路径，也可通过「选择图片…」导入本地图片。导入的图片保存为 `source/images/qsnote-avatar.png`，生成、导出和发布时随站点输出，四套主题共用此设置。清空头像字段恢复主题默认头像；网络图片需要联网加载。

## 站长统计与自定义代码

在「站点配置 → 站长统计」粘贴统计平台提供的完整 HTML / JavaScript 代码（包括 `<script>` 标签），生成器会将其原样插入每个页面的 `</body>` 前。可使用 CNZZ、百度统计等平台提供的嵌入代码，无需配置平台类型或站点 ID；清空即可关闭。

「站点配置 → 自定义代码」提供两个多行输入框：

- **Head 代码**：插入 `</head>` 前，可填写 `<meta>`、`<link>`、`<style>` 或 `<script>` 标签。
- **Body 底部代码**：插入 `</body>` 前，位于主题脚本和统计代码之后，可填写 HTML 或带 `<script>` 标签的 JavaScript。

三个字段均自动保存到站点 `_index.json`，保留换行，清空后重新生成即可移除。代码不做 HTML 转义、不解析 QSNote 模板占位符；JavaScript 和 CSS 需要包含相应的 HTML 标签。设置适用于四套内置主题和自定义主题，覆盖首页、文章、分页、归档、分类、标签及普通页面。自定义主题的 `layout.html` 应包含完整的 `</head>` 和 `</body>` 结束标签，无需额外占位符。

预览、导出和发布使用相同的生成结果，预览时也会执行这些代码；统计平台和其他外部资源可能需要联网。生成本身仍可离线完成。代码只注入生成的 HTML 页面，不写入搜索索引或原样复制的附件。

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

正文与编辑器预览共用 Markdown-it、Prism 和 CSS，支持标题、列表、引用、表格、删除线、脚注、上下标、标记、提示块和代码高亮。原始 HTML 使用与预览相同的安全过滤规则。公式及 Mermaid、Flowchart、Graphviz、WaveDrom、PlantUML 图表在生成时固化为图片，阅读时无需渲染脚本或网络；PlantUML 需要本地引擎。渲染失败会报告文章并保留上次生成结果。个性化阅读设置仅影响编辑器。文章页保留分类、标签和上下篇链接，NexT 使用原来的侧栏标题目录，Yilia 使用右下角悬浮目录。站内 Markdown 链接及本地图片会按输出路径重写；`_posts` 内的图片和附件输出到 `assets/posts`。原始 HTML 中的图片和链接也使用相同的路径映射。

文章按日期降序排列，列表和归档每页 10 篇。搜索沿用主题原有界面：Pure 的弹层读取本地 `content.json`，NexT 的搜索弹层读取 `search.json`，Yilia 的「所有文章 / 搜索」侧栏读取 `content.json`，支持标题、正文及 `#标签`。三套主题均可离线搜索。Pure 保留原来的作者导航栏、内容区、右侧组件和底部翻页栏；NexT 保留 Muse 的顶部导航、文章列表、标题区和可切换的目录侧栏。Yilia 保留固定左栏、文章卡片、按年份排列的归档与手机侧栏。响应式断点、字体、间距和颜色沿用原主题 CSS。Yilia 的交互使用本地适配脚本，不加载上游访问上报；站点信息、评论和页脚沿用 QSNote 配置。

生成先写临时目录，完成后替换 `public` 并清除过期页面；解析、路径冲突或写入失败时保留上一次输出。

## 预览和发布

预览使用只监听本机回环地址的临时 HTTP 服务和 Qt WebEngine，关闭预览即停止服务。域名支持子路径，例如 `https://example.com/blog/`，预览和输出中的站点链接使用相同的 `/blog/` 前缀。

正文渲染结果保存在系统的应用缓存目录，Linux 默认路径为 `~/.cache/qiushao/qsnote/sites/<站点路径哈希>/articles/`（遵循 `XDG_CACHE_HOME`）。缓存按站点绝对路径隔离，不写入站点目录。再次预览或发布时，未修改的文章复用公式、图表和 Markdown 渲染结果，重启应用后仍有效。正文、内置渲染资源、Qt 版本或界面语言变化时重新渲染；主题、文章属性、站内链接和附件每次按当前内容生成。含 `plantuml` 的正文不使用缓存，以免遗漏外部 include 文件的变化。首次生成仍需完整渲染；缓存不发布，可以删除，缺失、格式损坏或不可写时自动回退到正常渲染。在应用中删除文章或章节时同步清除自身及子文章的缓存；确认关闭笔记本并保存成功后，清除该笔记本的全部缓存，源文件和已有导出结果保留。重新打开后首次生成会重新建立缓存。

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

`qsnote_site` 在清空 PATH 后验证 Pure/NexT/Yilia 生成，并覆盖文章与元数据、分页、分类标签、归档、搜索、评论、资源和站内链接、中文子路径、失败保留输出和过期页面清理。`qsnote_publish` 验证本地 Git 发布及模拟 SSH/SCP 参数；`qsnote_ui` 验证三套主题的 WebEngine 预览、实际搜索交互、手机菜单开合、配置持久化及发布入口。主题 CSS 的 SHA-256 校验可防止原有布局和配色被意外改动。资源来源见 `src/resources/site/README.md`。

自定义主题测试使用上述示例，覆盖无外部程序生成、相对/完整目录、模板回退、分页和分类标签、文本转义、资源复制、切换主题后的旧资源清理，以及无效模板/预处理资源/输出冲突时保留上次输出。UI 测试验证主题目录设置持久化、字段显隐与实际 WebEngine 预览。
