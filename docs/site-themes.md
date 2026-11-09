# 站点主题规范（版本 1）

[教程目录](README.md) · [静态站点与发布](static-sites.md)

内置 Pure、NexT、3-Jekyll、Shiro、Plume 和外部主题使用同一个渲染器。主题名称仅用于定位资源。生成器提供数据，`theme.json` 声明行为和入口，HTML 模板负责布局、类名、图标及组件显隐。无需 Hexo、Node.js 或模板运行时。

## 启用示例

1. 复制完整的 [examples/site-theme](../examples/site-theme/) 到笔记本的 `themes/my-theme/`。
2. 在“站点配置 → 主题设置”选择“自定义”，主题目录填 `themes/my-theme`，也可选择绝对路径。
3. 保存并预览。相对目录以笔记本根目录为基准。

也可以复制任一 `src/resources/site/<主题>/` 目录，以任意名称作为外部主题使用，无需修改 C++。

## 目录和配置

```text
my-theme/
├── theme.json
├── layout/          # HTML 模板，支持子目录
│   ├── layout.html
│   ├── index.html
│   ├── post.html
│   ├── page.html
│   └── ...
└── source/          # 编译好的 CSS、JavaScript、图片等
```

示例配置：

```json
{
  "version": 1,
  "templates": "layout",
  "assets": "source",
  "entries": {
    "layout": "layout",
    "index": "index",
    "post": "post",
    "page": "page",
    "archive": "index",
    "category": "index",
    "tag": "index",
    "categories": "categoryIndex",
    "tags": "tagIndex",
    "links": "links",
    "document": "page",
    "documentNode": "documentNode"
  },
  "options": {
    "excerptLength": 128,
    "excerptFormat": "text",
    "singlePostIndex": false,
    "groupByYear": [],
    "searchEscapeFields": []
  }
}
```

`version` 必须为数字 `1`。`templates`、`assets` 是主题根目录内的相对目录，不能包含 `..`、绝对路径或空路径段。`entries` 中所有键必填，值是不含 `.html` 的模板路径；多个入口可指向同一个模板，没有隐式回退。配置中未知键、错误类型、不存在或空的模板都会报错，并注明文件路径。

| 入口 | 用途 |
| --- | --- |
| `layout` | 所有页面的外壳，使用 `{{{content}}}` 插入已渲染的主体 |
| `index` | 首页和首页后续分页 |
| `post` / `page` | 文章 / 普通独立页面 |
| `archive` | 全站及年月归档 |
| `category` / `tag` | 某分类 / 某标签下的文章列表 |
| `categories` / `tags` | 分类 / 标签总览页 |
| `links` | 友链页 |
| `document` | 文档及文档集合目录页 |
| `documentNode` | 文档目录的单个节点，生成器从叶到根渲染，无需递归引用模板 |

`options` 可省略，各字段独立生效：

| 选项 | 默认值及含义 |
| --- | --- |
| `excerptLength` | `128`，文本摘要长度，至少为 3；超长时末尾三个字符为 `...` |
| `excerptFormat` | `text`；`html` 使用正文 `<!-- more -->` 前已渲染内容，没有该标记则为空 |
| `singlePostIndex` | `false`；首页某页只有一篇文章时提供 `singlePost`，模板决定如何展开 |
| `groupByYear` | `[]`；可列出 `index`、`archive`、`category`、`tag`，为这些列表提供按年份降序的 `groups` |
| `searchEscapeFields` | `[]`；可列出 `title`、`content`，对搜索 JSON 的这些字段进行 HTML 转义，适配已有搜索客户端 |
| `defaultAvatar` | 空字符串；未设置头像时使用的资源路径 |
| `rootRelativeAvatar` | `false`；是否把 `/` 开头的站内头像路径也加上部署子路径 |

视觉选项应直接写在模板中；不要把主题名字作为行为模式。分页大小和站点 URL 规则仍由公共生成器管理。

## 模板语法

普通插值默认 HTML 转义，包括单引号和双引号。对象和数组不能直接当作字符串输出，缺少的字段输出空字符串。

```html
<h1>{{title}}</h1>
<a href="{{url}}">{{site.title}}</a>
<div class="markdown-body">{{{body}}}</div>
```

三重花括号表示原始 HTML，适用于生成器提供的正文、目录、评论、已渲染片段及用户配置的 HTML 页脚。不要用它插入标题、标签名、URL 或其它普通文本。插入值不会再次作为模板解析，正文中的 `{{title}}` 保持原样。

条件、循环和对象作用域采用同一种区块语法，允许嵌套：

```html
{{#site.author}}<p>{{site.author}}</p>{{/site.author}}
{{#posts}}
  <article>
    <h2><a href="{{url}}">{{title}}</a></h2>
    {{#hasTags}}<ul>{{#tags}}<li>{{name}}</li>{{/tags}}</ul>{{/hasTags}}
  </article>
{{/posts}}
{{^posts}}<p>暂无文章</p>{{/posts}}
```

- 布尔值按真假判断；数字 `0`、空字符串、空数组、空对象、null 和缺失字段为假。字符串 `"false"` 为真。
- `{{#数组}}` 对每个元素执行一次；`{{.}}` 引用当前元素。需要为整个列表加一个容器时，使用 `hasTags`、`hasCategories` 等布尔字段。
- `{{#对象}}` 将对象设为当前作用域；字段先查当前作用域，再查外层。布尔值或字符串条件不改变作用域。
- `{{^字段}}` 在字段为假时执行。结束块必须与开始块同名。
- `{{>card}}` 静态引用 `card.html`，沿用当前作用域。允许引用子目录片段，但不允许直接或间接引用环，也不支持动态模板名。
- `{{date|date:yyyy-MM-dd}}` 用 Qt 日期格式展示 ISO 日期；例如 `M月 d` 或 `MM-dd`。月份名称可显式指定区域：`{{date|date(zh_CN):MMMM yyyy}}`。不支持其它过滤器、表达式、EJS/Nunjucks 或任意脚本执行。

所有模板在加载时解析并检查区块和片段引用。字段名可以包含点号访问对象；未知字段视为缺失值，因此修改字段名后应预览确认。

## 数据上下文

| 范围 | 字段 |
| --- | --- |
| 全站 `site` | `root`、`title`、`author`、`city`、`githubSocial`、`avatar`、`description`、`footer`、`domain`、`postCount`、`categoryCount`、`tagCount` |
| 全站列表 | `site.posts`、`site.recent`（前 5 篇）、`site.categories`、`site.tags`、`site.archives`、`site.friends`、`site.navigation` |
| 当前页面 | `title`、`headTitle`、`route`、`url`、`permalink`、`keywords`、`body`；单页还提供完整 `page` 对象 |
| 文章对象 | `id`、`title`、`url`、`route`、`permalink`、`date`、`updated`、`body`、`text`、`excerpt`、`toc`、`post`、`document`、`pinned`、`tags`、`categories`、`hasTags`、`hasCategories`、`wordCount`、`readingMinutes` |
| 单篇补充 | `comments`、`previous`、`next`（标题和 URL）、`hasNavigation`、`showNavigation` |
| 列表 | `posts`（当前页）、`listCount`（总篇数）、`subsetCount`（当前页篇数）、`listTitle`（是否需要列表标题）、`groups`（年份和文章）、`singlePost`、`card`、`categoryListing`、`tagListing`、`terms` |
| 分页对象 `pagination` | `number`、`count`、`multiple`、`previous`、`next`（URL 或空）、`pages`（`number`、`url`、`current`） |
| 文档 | `collection`（`title`、`url`）、`parents`（上级目录名数组）、`documentTree`（已渲染目录节点） |
| 目录节点入口 | `title`、`folder`、`children`（已渲染子节点）、`url`、`active` |

全站标量字段同时提供顶层别名，其中站点标题使用 `siteTitle`，避免与页面 `title` 冲突。外壳的 `content` 是入口模板的已渲染 HTML，`body` 始终是原正文。列表文章不包含评论或前后篇数据。

文章的分类／标签条目包含 `name`、`url`；标签还包含稳定的 `color`（0–5）及 `label`（是否属于前 3 个）。全站分类／标签条目另含 `count`、`posts`、`selected`；`terms` 对应当前分类／标签列表，`selected` 标识匹配的条目。归档条目包含 `date`（该月首日的 ISO 日期）、`url`、`count`，友链包含 `title`、`url`。

`site.navigation` 包含 `title`、`icon`（已校验的 SVG）、`url`、`active`，分组包含 `group` 和 `children`。未配置导航时自动提供首页、文档集合及站点索引入口。`site.posts` 还提供 `active` 和属性用的 `tagsJson`，可用于客户端筛选。

`view` 提供布尔字段：`home`（仅第一页）、`index`、`listing`、`archive`、`taxonomy`、`page`、`post`、`document`、`about`、`links`、`categories`、`tags`。`postToc` 为文章目录，`navToc` 是使用 `nav-` 类名前缀的同一目录，供已有侧栏脚本使用。`rootJsonValue` 和 `searchJsonValue` 是经过脚本上下文编码的 JSON，供 NexT 的配置脚本使用。

## 资源、搜索与自定义代码

`assets` 指定目录的内容复制到输出根目录。所有主题统一忽略以 `.` 或 `_` 开头的路径段；发现 Stylus、Sass、Less、EJS、Nunjucks、Pug、Swig 等待编译文件时拒绝生成。主题目录中的 `LICENSE` 和 `THIRD-PARTY-NOTICES.md` 会随输出保留。

使用 `{{root}}` 构建资源路径，它包含部署子路径：

```html
<link rel="stylesheet" href="{{root}}css/qsnote-article.css">
<link rel="stylesheet" href="{{root}}css/theme.css">
```

保留公共正文样式引用，以支持代码、公式和图表。资源 CSS、JavaScript 不做预处理。生成的 `search.json` 和 `content.json` 可供主题脚本使用；示例主题没有搜索界面。评论继续使用站点配置，`{{{comments}}}` 输出已生成代码；head/body 自定义代码在最终外壳渲染完后注入。

## 旧主题迁移

**没有 `theme.json` 的旧主题会明确报错，不再隐式沿用旧实现。**

1. 添加版本 1 配置，显式映射所有入口；原先可选的归档／分类／标签模板可直接映射到 `index`。
2. 将正文、页脚和评论的旧 `{{body}}` 等改为三重花括号；普通文本维持双重花括号，不要自行预转义。
3. 外壳用 `{{{content}}}`；列表入口用 `{{#posts}}{{>card}}{{/posts}}`，原 `body` 不再是卡片集合。
4. `meta`、分类标签链接、分页和前后篇 DOM 改为主题片段；参考完整示例的 `meta.html`、`categories.html`、`tags.html`、`pagination.html`、`navigation.html`。原 `avatarImage`、`articleStyle` 等 DOM 占位符改为模板中的 HTML。
5. 为分类／标签总览、友链、文档和文档节点显式提供入口。只有声明为入口或被片段引用的模板会用于对应页面。
6. 检查空站点、多页列表、中文与特殊字符、子路径、目录、评论、窄窗口和主题切换。始终修改主题源目录，`public/` 会在成功生成后被替换。

## 开发验证

```bash
cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

测试覆盖模板解析和非法配置、五主题 250 份固定内容基线、完整生成流程、外部改名主题、资源规则、输出清理、桌面／移动视口及搜索。浏览器检查使用项目已有的 Qt WebEngine，截图写入 `build/tests/screenshots/`。固定基线比较忽略无意义 HTML 空白和属性顺序，保留结构、属性、链接、文本及搜索 JSON。完整生成的改名主题比较使用相同的 Markdown 缓存状态：现有正文渲染器冷、热缓存可能产生空白差异，本次不修改该逻辑。
