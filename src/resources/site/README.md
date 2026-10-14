# 原主题静态资源

资源来源是 QSNote 提交 `9752f93` 中内置的 Pure（package 版本 0.0.1）和 Stellar（1.44.0），不是重新设计的主题。

- `templates/`：从原主题页面提取的 HTML 片段，保留 DOM、类名和内联图标；`{{name}}` 由 C++ 生成器填充。
- `assets/`：原 CSS、字体、图片和浏览器脚本。Pure 的 `style.css` 直接复制；Stellar 的 `main.css` 使用原站点配置一次性编译得到，程序构建和运行均不需要 Stylus、Hexo、Node.js 或 npm。
- Stellar 的默认头像使用原 CDN 的同一 SVG，改为本地资源。
- 两套主题的 `js/highlight.js` 使用原依赖 highlight.js 11.12.0 的 common 语言集，包含浏览器包装代码，作为原主题资源保留；文章正文现在使用与预览共享的 Prism 高亮和正文样式。许可证见相邻的 `highlight.LICENSE`。

上游许可证保存在各主题的 `LICENSE`，Stellar 的额外声明见 `THIRD-PARTY-NOTICES.md`，lazyload 的许可证保存在 `assets/js/vendor/`。CSS 和脚本中的原有版权声明保留。

原样式 SHA-256（生成测试同时检查）：

| 文件 | SHA-256 |
| --- | --- |
| `pure/assets/css/style.css` | `2aab838134abc02a55c42e3afcee6e867155aca4b0c28c7d66d1e1f74d0b9c10` |
| `stellar/assets/css/main.css` | `f98026219c14ce4edfa2b80f76d01bb1349ae6c5c7a37557d8d2eb66f9aa0e73` |

还原时使用同一组 12 篇文章，对照原生成器与 C++ 生成器的首页、文章、分类、标签、归档、友链、关于页面，在 1440×900 和 390×844 下截图并检查主要区域的布局与计算样式。持续测试覆盖原 CSS 指纹、页面结构、两套搜索界面和手机菜单交互。
