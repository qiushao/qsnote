# 原主题静态资源

Pure 来自 QSNote 提交 `9752f93` 中内置的版本（package 版本 0.0.1）。
NexT 来自 [next-theme/hexo-theme-next](https://github.com/next-theme/hexo-theme-next)，版本 8.29.0，固定提交 `d25776ea38ddc7269fe9c3b94b6fd5cfcaae20e3`，使用 Muse 布局。

- `theme.json`：声明规范版本、入口模板、资源目录及已有行为差异；内置与外部主题采用同一规范。完整配置、上下文和迁移说明见 [主题规范](../../../docs/site-themes.md)。
- `templates/`：按上游模板 DOM 和类名适配的 HTML，包含文章、归档、分类、分页、导航和侧栏。生成器只提供通用数据，普通 `{{name}}` 默认 HTML 转义，原始 HTML 使用 `{{{body}}}`，静态片段使用 `{{>meta}}`。
- `{{githubSocial}}` 是用户设置的 GitHub 地址（HTML 转义后），各主题在模板中定义链接、图标和布局。`{{#githubSocial}}…{{/githubSocial}}` 仅在地址非空时输出内容；未填写或非 HTTP(S) 地址不显示。区块支持嵌套、对象作用域与列表遍历。城市通过 `{{city}}` 填充，只用于原主题已有的城市位置。
- `assets/`：原 CSS、字体、图片和浏览器脚本。Pure 的 `style.css` 直接复制；NexT 的 `main.css`、`noscript.css` 由上游 Stylus 预编译，程序构建和运行均不需要 Stylus、Hexo、Node.js 或 npm。
- NexT 的 `config.js`、`utils.js`、`sidebar.js`、`next-boot.js` 和 `third-party/search/local-search.js` 原样复制到 `assets/js/`。搜索依赖 `hexo-generator-searchdb` 1.5.0 的 `dist/search.js`，图标使用 Font Awesome Free 7.3.1 的 CSS 与 WOFF2 字体，默认头像来自上游 `source/images/avatar.gif`。所有运行时资源均在本地。
- NexT 的页面和搜索数据由 QSNote 提供，正文沿用编辑器预览的 Prism 高亮和默认样式，评论沿用站点配置。搜索标题和正文在写入索引时做 HTML 转义，以匹配搜索组件插入 HTML 的方式。
- Pure 的 `js/highlight.js` 使用原依赖 highlight.js 11.12.0 的 common 语言集，作为原主题资源保留，许可证见相邻的 `highlight.LICENSE`。

许可证：Pure 见 `pure/LICENSE`；NexT 为 AGPL-3.0-only，见 `next/LICENSE`；搜索依赖见 `next/assets/js/search.LICENSE`；Font Awesome 的代码、字体与图标授权见 `next/assets/lib/fontawesome/LICENSE.txt`。上游源码可从固定提交获取，QSNote 的模板适配和生成逻辑位于本目录与 `src/core/SiteGenerator.cpp`。

## NexT CSS 的生成方式

仅更新内置主题时需要 Node.js 22、Hexo 8.1.2、hexo-renderer-stylus 3.0.1（Stylus 0.62.0）。将固定提交放入临时 Hexo 站点的 `themes/next`，站点配置设为 `theme: next`，`_config.next.yml` 使用以下覆盖项，其余使用上游默认值：

```yaml
scheme: Muse
motion:
  enable: false
font:
  enable: false
local_search:
  enable: true
  preload: true
menu:
  home: / || fa fa-home
  categories: /categories/ || fa fa-th
  tags: /tags/ || fa fa-tags
  archives: /archives/ || fa fa-archive
  links: /links/ || fa fa-link
  about: /about/ || fa fa-user
```

执行 `hexo generate` 后复制 `public/css/main.css` 与 `public/css/noscript.css`。关闭网页字体下载和入场动画；无需对 CSS 做额外修改。更新时同时检查模板与上游脚本需要的 DOM 和配置字段。

原样式 SHA-256（生成测试同时检查）：

| 文件 | SHA-256 |
| --- | --- |
| `pure/assets/css/style.css` | `2aab838134abc02a55c42e3afcee6e867155aca4b0c28c7d66d1e1f74d0b9c10` |
| `next/assets/css/main.css` | `d5d3280e2cb167a84fca9c4459d6a5db0603b2512f201ce85bd79b6148d9d71e` |

## 3-Jekyll 的接入方式

- 来源：[P233/3-Jekyll](https://github.com/P233/3-Jekyll)，固定提交 `45ca7c4578d498890c8ef078ff13c189a12df114`，对应 master 的三栏版本。
- 保留上游默认绿色渐变、170px 标签栏、380px 文章列表栏、正文区，以及 1280px / 767px 响应式断点。模板对应上游 `default.html`、`post.html` 和 `sidebar.html`；默认头像和 IcoMoon 图标字体原样复制。
- `main.css` 从上游 Sass 预编译；`qsnote-3-jekyll.css` 仅补充站点导航、列表、分页及键盘焦点。正文继续使用 QSNote 的统一 Markdown 样式与高亮。
- 上游首页只有 `hi` 占位，移植后使用站点标题和分页文章摘要。左侧按标签筛选，文章列表包含全部已发布文章；搜索标题、正文和标签，支持中文、带空格或引号的标签。分类、归档、友链、关于及评论沿用现有站点数据。
- `qsnote-3-jekyll.js` 适配筛选、搜索与手机菜单；页面使用静态链接跳转，避免 PJAX 替换正文时影响评论加载。未引入上游 Google Analytics、远程网页字体或旧版 JS 依赖；生成与浏览均无需 Jekyll、Ruby、Node.js 或 npm。
- 主题标识为 `3-jekyll`，在站点配置的主题下拉框中选择 **3-Jekyll**。上游此提交未附主题许可证，来源及授权现状如实记录在 `3-jekyll/THIRD-PARTY-NOTICES.md`，该文件也随站点输出。

CSS 更新时，在上述固定提交的源码目录中去掉 `css/main.sass` 开头的 Jekyll front matter（前 3 行），另存为 `main-build.sass`，然后执行：

```sh
npm exec --yes --package=sass@1.80.7 -- sass --no-source-map --style=compressed --load-path=_sass main-build.sass main.css
```

将生成文件复制到 `3-jekyll/assets/css/main.css`。不改动 Sass 变量或导入顺序。
CSS SHA-256：`2ed7ba6e517eea794bb4e71edd77e3b891c4ba438b9e61e30630e9229f5b0db9`。
测试覆盖原生离线生成、配置保存、子路径、分页、评论、主题切换清理，以及桌面三栏、筛选、全文搜索、手机菜单和 Escape 关闭；预览测试输出 `site-preview-3-jekyll*.png`。

## Shiro 的接入方式

- 来源：[Acris/hexo-theme-shiro](https://github.com/Acris/hexo-theme-shiro)，版本 1.6.1，固定提交 `417329cba818706c989655b55d5c1b6616244637`，MIT 许可。`LICENSE` 和 `THIRD-PARTY-NOTICES.md` 随站点输出。
- `style.min.css`、`toc.min.css` 及明暗切换、手机菜单、目录、阅读进度、返回顶部脚本原样复制上游已编译文件。保留雾色背景、居中纸张、朱红印章、衬线字体栈和响应式目录；字体回退到本机字体，不请求 Google Fonts。
- HTML 片段由上游 Nunjucks 模板适配，文章、分页、分类、标签、归档、友链、独立页面和评论来自 QSNote。正文保留统一 Markdown、Prism、公式与图表样式，适配层让正文背景和文字跟随 Shiro 明暗模式。
- `qsnote-shiro.js` 将中文目录片段转换为上游脚本需要的目标 ID，并通过生成器的 `search.json` 提供标题/全文搜索；使用文本节点展示结果。搜索对话框支持键盘焦点恢复，手机菜单沿用上游 Escape 关闭和折叠后的 inert 行为。
- 不引入 Pagefind、远程统计、LightGallery 或字体加载遮罩；不执行 Hexo 插件或读取 `_config.yml`。构建与生成均不需要 Node.js。更新上游资源的命令见 `shiro/THIRD-PARTY-NOTICES.md`。
- 站点配置中选择 **Shiro**，保存后重新预览。站点主题标识为 `shiro`。

原 CSS SHA-256：`993ff721b9f1dff4e13fd6f233068a78680ec177f63c8ed46da9b348fba986d6`。

使用 `-DQSNOTE_BUILD_TESTS=ON` 配置并构建后运行 `ctest --test-dir build --output-on-failure`。测试覆盖五主题的固定输出、原生生成、外部改名主题、资源规则及主题切换，并使用 Qt WebEngine 检查桌面／移动视口和搜索。截图写入 `build/tests/screenshots/`。
