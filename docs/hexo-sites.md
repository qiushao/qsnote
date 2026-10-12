# Hexo 静态站点

QSNote 静态站点默认使用 Pure 主题，可在「站点配置 → 主题与页脚 → 站点主题」中切换到 Stellar；保存后保留选择。创建和保存站点不需要 Node.js；预览和发布需要 PATH 中的 Node.js 18+ 和 npm。首次生成在站点目录执行 `npm ci --no-audit --no-fund` 安装锁定的依赖，之后可离线生成默认站点。首次使用 Stellar 时还会在 `themes/stellar` 中执行 `npm ci --no-audit --no-fund` 安装主题依赖，不修改站点根目录的依赖文件。启用第三方评论服务或使用主题的远程资源时仍需联网。

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
├── themes/stellar/         # 含独立的 package.json 和锁文件
├── node_modules/           # 首次生成时安装
└── public/                 # Hexo 生成结果
```

Pure 与 Stellar 的模板、脚本、字体、图片、语言文件及许可证编译在 Qt 资源 `src/resources/hexo.qrc` 中，创建站点时释放。重新保存不会覆盖已存在的主题、页面或 scaffold 文件。`_config.yml` 使用 YAML 支持的 JSON 写法，包含配置界面中的站点信息、评论和页脚设置；友链写入 `source/_data/links.json`。这两个文件由 QSNote 维护，请在界面中修改对应设置。可在 `themes/<主题名>/_config.yml` 中修改其他主题选项。

保存旧站点时，原有文章移到 `source/_posts/分类/标题.md` 并补充 Hexo front matter，草稿保存在 `source/_drafts`，预览和发布均不发布草稿。原来的 minimal、notes、tech 配色统一替换为 Pure，已选择的 Stellar 保留。正文和索引中的元数据保留。应用管理的 front matter 包含标题、日期、slug、分类和标签，不在普通正文编辑区显示；元数据修改会同步回文章文件。如果直接在正文中编写完整的 YAML front matter，则使用作者提供的 front matter。

## 预览和发布

在站点配置的「评论」中可选择关闭评论、Valine、Gitalk 或 Disqus。新建及未设置评论平台的站点默认关闭评论，已有明确选择的站点保留原设置。仅显示当前选中评论系统的设置项，关闭评论时隐藏所有系统的设置项。使用 Valine 时，填写对应 LeanCloud 应用的 App ID 和 App Key；Pure 的其他 Valine 选项沿用 `themes/pure/_config.yml` 中的设置。Stellar 同样支持界面中的三种评论服务及关闭评论。

预览与发布调用相同的 Hexo API：`init` → `clean` → `generate`。EJS 页面、Markdown、首页分页、归档、分类、标签和搜索索引由站点安装的 Hexo 插件生成。每次生成会清理 `public` 中的旧产物，请将需要发布的自定义文件放在 `source` 中。

预览使用只监听本机回环地址的临时 HTTP 服务和 Qt WebEngine，关闭预览即停止服务。站点域名支持子路径，例如 `https://example.com/blog/`；预览会使用同样的 `/blog/` 路径。

在「站点配置 → 生成与发布 → 发布方式」中选择 **GitHub Pages** 或 **SSH SCP**，只显示对应的设置项；切换方式保留已填写设置。新建和旧站点默认未选择发布方式，不影响预览。点击「发布站点」后，应用先校验设置，再生成并上传完整 `public` 内容（不含草稿、应用索引、依赖和发布配置）。

- **GitHub Pages**：填写仓库 SSH/HTTPS 地址和发布分支（留空为 `gh-pages`）。需要 PATH 中的 `git`，认证使用本机 Git 凭据或 SSH 密钥。首次发布创建分支，后续发布保留提交历史、替换该分支根目录的内容并正常推送；推送冲突或认证失败会提示失败。应用自动添加 `.nojekyll`。在 GitHub 仓库 **Settings → Pages → Deploy from a branch** 选择发布分支和 `/ (root)`。推送完成后仍需等待 GitHub 部署；应用不自动修改仓库 Pages 设置。站点「域名」填写实际 URL，项目站点须包含仓库子路径，例如 `https://user.github.io/repo/`。自定义域名需要的 `CNAME` 放在 `source/CNAME`。参见 [GitHub Pages 发布来源设置](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)。
- **SSH SCP**：填写主机（域名、IP 或 SSH 配置别名）、端口（留空为 `22`）、用户名、远程绝对目录和可选私钥文件完整路径。远程目录支持文字、数字、下划线、短横线、点和斜杠，不含空格或 shell 特殊字符。需要 PATH 中的 `ssh`、`scp`，通过密钥或 ssh-agent 认证；先在终端连接主机并确认指纹。发布时创建目标目录，上传 `public` 的内容（含隐藏文件），覆盖同名文件，保留远端其他文件；删除的文章不会自动从服务器删除。参见 [OpenSSH scp 手册](https://man.openbsd.org/scp)。

发布采用非交互认证，不在应用内收集或保存密码、Token 或私钥内容。设置保存在站点 `_index.json` 中；SSH 私钥字段只保存路径。每条发布命令最多等待 5 分钟，失败会显示命令错误；上传过程中失败可能留下部分已更新的文件，修复连接或权限后可重新发布。

站点也可以独立使用 Hexo CLI：

```sh
cd 站点目录
npm ci
# 使用 Stellar 时额外安装主题依赖：
# npm ci --prefix themes/stellar
npm run build
npm run server
```

Hexo 目录与配置说明：[Setup](https://hexo.io/docs/setup)、[Configuration](https://hexo.io/docs/configuration)。

## Pure 移植

来源：用户指定的本地 `hexo-theme-pure`，上游为 [cofess/hexo-theme-pure](https://github.com/cofess/hexo-theme-pure)。许可证随主题保留。

应用保留 Pure 的 EJS 布局和资源，仅调整默认示例信息、默认导航和第三方服务开关；jQuery 使用主题自带文件，favicon 支持站点根路径，页脚显示应用设置的文字，未设置友链头像时使用本地默认图片。

## Stellar 移植

来源：[xaoxuu/hexo-theme-stellar](https://github.com/xaoxuu/hexo-theme-stellar)，锁定 npm 发布版 **1.44.0**，MIT 许可证及第三方声明随主题保留。为兼容当前 Node.js 18 / Hexo 7，不采用要求升级运行环境的 Stellar 2.x。

保留上游布局、样式、脚本和本地搜索，接入 QSNote 的标题、导航、分类、标签、友链、关于、页脚和评论设置。Pure 与 Stellar 共用文章和页面，切换不覆盖用户编辑的主题、页面及 scaffold 文件。旧 Pure 站点保存后会补齐 Stellar 资源，首次切换时安装主题依赖。

相对上游的适配：

- 主题依赖单独锁定，Cheerio 使用兼容 Node.js 18 的 1.0.0，显式声明脚本使用的 hexo-util；移除开发依赖和开发脚本。
- 增加 `about`、`links` 布局和 Valine、Gitalk、Disqus 评论适配。
- 修正自定义文章 slug 在搜索索引中产生双斜杠的问题，确保子路径站点中的搜索链接可访问。
- 图片懒加载库 vanilla-lazyload 19.1.3 随主题提供，并保留许可证；基础样式、脚本和搜索可在本地预览中加载。
- 默认关闭依赖远程脚本的 ScrollReveal 动画，避免网络不可用时正文被隐藏。
- 关闭生成时的上游版本查询，避免每次生成依赖网络。可选远程组件和评论服务仍按主题配置联网加载。

验证命令：

```sh
cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`qsnote_hexo` 覆盖创建、旧站点迁移、front matter、Pure/Stellar 双向切换、真实 Hexo 生成、子路径资源、搜索、评论、生成内容和旧页面清理；`qsnote_publish` 使用本地 Git 裸仓库验证首次/重复发布、分支更新与旧文件清理，并使用模拟 SSH/SCP 命令验证参数和失败处理；`qsnote_ui` 覆盖发布设置联动与持久化、真实站点生成后推送本地仓库，以及主题选择持久化、两种主题的 WebEngine 站点预览和发布入口。
