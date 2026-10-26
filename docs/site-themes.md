# 自定义站点主题

[教程目录](README.md) · 上一篇：[静态站点与发布](static-sites.md)

本篇适合已经能预览站点、了解基本 HTML/CSS 的用户。使用仓库中的[示例主题](../examples/site-theme/)起步，避免从空目录搭建。

## 1. 先启用示例主题

1. 复制仓库的 `examples/site-theme/` 整个目录到站点笔记本下，例如 `themes/my-theme/`。
2. 打开“站点配置 → 主题与页脚”，站点主题选“自定义”。
3. 主题目录填 `themes/my-theme`，或通过“选择主题目录…”选择完整路径。
4. 关闭配置，点击“站点预览”。

预期结果：站点使用示例中的导航、文章卡片、正文和页脚。相对主题目录以站点笔记本根目录为基准；为了迁移方便，可将主题一起放在笔记本内备份。

主题采用 Hexo 风格的 `layout/`、`source/` 目录结构，但模板格式是 QSNote 自己的 HTML 占位符。不能直接选择任意 Hexo 主题就运行：EJS、Nunjucks、Stylus/Sass 等需先转换或编译；程序不执行主题插件，也不读取主题 `_config.yml`。

## 2. 理解五个必需模板

```text
my-theme/
├── layout/
│   ├── layout.html    # 页面外壳
│   ├── index.html     # 首页和列表
│   ├── card.html      # 列表中的文章卡片
│   ├── post.html      # 文章页
│   └── page.html      # 独立页面
└── source/
    └── css/theme.css
```

这五个模板必须存在且不为空。`archive.html`、`category.html`、`tag.html` 是可选的对应列表模板；不提供时使用列表回退模板。

生成器先准备文章和列表内容，再将其填入模板，最后套到 `layout.html` 的 `{{body}}`。同一个占位符在不同模板中可以有不同含义，例如外壳的 `body` 是完整页面主体，文章模板中的 `body` 是文章正文。

## 3. 第一次改样式

打开复制后的 `source/css/theme.css`，在末尾添加：

```css
.site-title {
  color: #3567c7;
  letter-spacing: 0.08em;
}
```

重新预览，预期示例主题的站点标题颜色与字距改变。`layout/layout.html` 里的样式引用应保持：

```html
<link rel="stylesheet" href="{{root}}css/theme.css">
```

主题 `source/` 内的文件复制到生成站点根目录，`{{root}}` 会包含站点子路径。若站点发布在 `/learning/`，上述链接就指向 `/learning/css/theme.css`；不要固定写成 `/css/theme.css`。

## 4. 常用占位符

| 用途 | 占位符 | 放置示例 |
| --- | --- | --- |
| 站点信息 | `siteTitle`、`description`、`author`、`city`、`footer`、`domain` | 外壳标题与页脚 |
| 路径与身份 | `root`、`githubSocial`、`avatar`、`avatarImage` | 导航、个人链接、头像 |
| 页面头部 | `headTitle`、`keywords`、`permalink`、`articleStyle` | `layout.html` 的 head 内 |
| 通用内容 | `title`、`body` | 页面或文章标题、正文 |
| 文章卡片 | `url`、`date`、`meta`、`categories`、`tags`、`excerpt` | `card.html` 的摘要与链接 |
| 文章页 | `toc`、`navigation`、`comments` | `post.html` 的目录、前后篇和评论 |
| 列表 | `pagination`、`pageNumber`、`pageCount`、`postCount` | `index.html` 的分页与数量 |

这些占位符是允许的名称集合，具体是否有值取决于所在模板。没有值时输出空字符串；不支持自定义变量、循环或表达式。普通文本由生成器转义，`body`、`meta`、`articleStyle` 等包含已生成的 HTML，直接放在对应位置即可。保留示例外壳的 `{{articleStyle}}`，让正文、公式、代码等获得必要样式。

练习：修改 `card.html`，在文章标题下面显示日期：

```html
<article class="card">
  <h2><a href="{{url}}">{{title}}</a></h2>
  <time>{{date}}</time>
  <p>{{excerpt}}</p>
</article>
```

预期结果：首页每张卡片出现文章日期。保持 `index.html` 中的 `{{body}}` 与 `{{pagination}}`，否则文章列表或翻页入口不会显示。

## 5. 条件块

变量有非空值时才显示一段内容，可使用成对的条件块：

```html
{{#githubSocial}}
<a href="{{githubSocial}}">GitHub</a>
{{/githubSocial}}

{{#toc}}
<details>
  <summary>文章目录</summary>
  {{toc}}
</details>
{{/toc}}
```

把第一段放进外壳页脚：站点配置不填 GitHub 地址时链接不出现，填写有效 HTTP(S) 地址后出现。第二段放在文章模板中，有目录时显示折叠目录。

条件块不能嵌套，开始和结束必须使用同一个变量名。`{{#author}}…{{/city}}`、未知占位符、`<% ... %>` 或 `{% ... %}` 都会导致模板校验失败。

## 6. 导航、搜索与静态资源

自定义导航可直接指向生成的页面：

```html
<nav>
  <a href="{{root}}">首页</a>
  <a href="{{root}}archives/">归档</a>
  <a href="{{root}}projects/">项目</a>
</nav>
```

“项目”需要先按[独立页面教程](static-sites.md)创建 `source/projects/index.md`。自定义主题示例不包含内置主题的交互搜索界面；需要搜索时自行编写前端并读取生成的搜索数据，或者继续使用内置主题。不要把“允许使用搜索数据”误认为模板会自动出现搜索框。

图片、编译后的 CSS 和 JavaScript 放进主题 `source/`，引用时使用 `{{root}}` 加相对路径。主题 CSS 和脚本不做预处理；先在外部编译好再放入。主题代码与“站点配置 → 自定义代码”可以同时生效，调试时需考虑两者叠加。

## 7. 验证与迁移

用至少两篇文章、一篇独立页面及一个标签练习，逐项检查：

1. 首页文章链接能打开，正文、图片、公式与图表正常。
2. 标签、分类、归档及前后篇链接有效；文章超过 10 篇时分页有效。
3. 将域名设为带子路径的测试值（如 `https://example.com/learning/`），重新本机预览，样式和导航仍正常。
4. 缩窄预览窗口，检查长标题、图片和表格没有挡住操作。
5. 没有作者、头像、GitHub 地址或评论时，不出现无意义的空区域。

模板错误时按报错文件检查必需文件、变量拼写和条件块。调整主题只修改原主题目录，再重新生成；直接修改 `public/` 会在下次生成时被替换。迁移时把 `themes/my-theme/` 一并复制，若使用绝对主题路径则需在新机器重新选择目录。
