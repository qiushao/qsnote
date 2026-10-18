# 原主题静态资源

Pure 来自 QSNote 提交 `9752f93` 中内置的版本（package 版本 0.0.1）。
NexT 来自 [next-theme/hexo-theme-next](https://github.com/next-theme/hexo-theme-next)，版本 8.29.0，固定提交 `d25776ea38ddc7269fe9c3b94b6fd5cfcaae20e3`，使用 Muse 布局。

- `templates/`：按上游模板的 DOM 和类名适配的 HTML 片段，`{{name}}` 由 C++ 生成器填充。NexT 对应 `_layout.njk`、`_macro/post.njk`、`_macro/sidebar.njk` 及导航、搜索、独立页面模板；归档、分类、分页由生成器按上游结构输出。
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

持续测试覆盖离线生成、CSS 指纹、配置迁移、分页、页面结构、搜索、目录和手机菜单。预览测试保存桌面与移动尺寸的截图供检查。
