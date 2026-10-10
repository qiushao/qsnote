# Hexo 静态站点

QSNote 静态站点默认使用 Pure 主题。创建和保存站点不需要 Node.js；预览和导出需要 PATH 中的 Node.js 18+ 和 npm。首次生成在站点目录执行 `npm ci --no-audit --no-fund` 安装锁定的依赖，之后可离线生成默认站点。启用第三方评论服务时仍需联网。

## 文件结构

```text
站点/
├── _index.json             # QSNote 笔记索引和应用设置
├── _config.yml             # Hexo 设置（由站点配置界面维护）
├── package.json
├── package-lock.json
├── qsnote-generate.cjs     # 应用调用的 Hexo 生成入口
├── scaffolds/
├── source/
│   ├── _posts/             # 文章，按分类存放
│   ├── _drafts/
│   ├── _data/links.json
│   ├── categories/index.md
│   ├── tags/index.md
│   ├── links/index.md
│   └── about/index.md
├── themes/pure/
├── node_modules/           # 首次生成时安装
└── public/                 # Hexo 生成结果
```

Pure 的模板、脚本、字体、图片、语言文件及许可证编译在 Qt 资源 `src/resources/hexo.qrc` 中，创建站点时释放。重新保存不会覆盖已存在的主题、页面或 scaffold 文件。`_config.yml` 使用 YAML 支持的 JSON 写法，包含配置界面中的站点信息、评论和页脚设置；友链写入 `source/_data/links.json`。这两个文件由 QSNote 维护，请在界面中修改对应设置。可在 `themes/pure/_config.yml` 中修改其他主题选项。

保存旧站点时，原有文章移到 `source/_posts/分类/标题.md` 并补充 Hexo front matter，草稿保存在 `source/_drafts`，预览和导出均不发布草稿。原来的 minimal、notes、tech 配色统一替换为 Pure。正文和索引中的元数据保留。应用管理的 front matter 包含标题、日期、slug、分类和标签，不在普通正文编辑区显示；元数据修改会同步回文章文件。如果直接在正文中编写完整的 YAML front matter，则使用作者提供的 front matter。

## 预览和发布

预览与导出调用相同的 Hexo API：`init` → `clean` → `generate`。EJS 页面、Markdown、首页分页、归档、分类、标签和搜索索引由站点安装的 Hexo 插件生成。每次生成会清理 `public` 中的旧产物，请将需要发布的自定义文件放在 `source` 中。

预览使用只监听本机回环地址的临时 HTTP 服务和 Qt WebEngine，关闭预览即停止服务。站点域名支持子路径，例如 `https://example.com/blog/`；预览会使用同样的 `/blog/` 路径。

点击「导出站点」选择目录后，应用将完整 `public` 内容复制到新目录，可以上传到 GitHub Pages、Nginx 等静态托管服务。导出包含 HTML、CSS、JavaScript、图片、字体和搜索索引，不包含应用索引、安装依赖或发布凭据。应用不自动上传。

站点也可以独立使用 Hexo CLI：

```sh
cd 站点目录
npm ci
npm run build
npm run server
```

Hexo 目录与配置说明：[Setup](https://hexo.io/docs/setup)、[Configuration](https://hexo.io/docs/configuration)。

## Pure 移植

来源：用户指定的本地 `hexo-theme-pure`，上游为 [cofess/hexo-theme-pure](https://github.com/cofess/hexo-theme-pure)。许可证随主题保留。

应用保留 Pure 的 EJS 布局和资源，仅调整默认示例信息、默认导航和第三方服务开关；jQuery 使用主题自带文件，favicon 支持站点根路径，页脚显示应用设置的文字，未设置友链头像时使用本地默认图片。

验证命令：

```sh
cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`qsnote_hexo` 覆盖创建、旧站点迁移、front matter、真实 Hexo 生成、子路径资源、导出内容和旧页面清理；`qsnote_ui` 覆盖配置、WebEngine 站点预览和导出入口。
