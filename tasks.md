# Code Review 后续任务

审查日期：2026-10-24。本文记录待执行任务，不代表已完成修复或重构。

审查基线：`MainWindow.cpp` 约 3,332 行、54 个成员函数，占自有 C++ 实现代码约 47%。重点处理冗余代码、重复实现与职责拆分，同时优先修复审查发现的数据风险。

## 执行约定

- 按任务逐个实施、验证和提交，完成后勾选任务并填写验证记录。
- 优先级：P1 为数据风险；P2 为行为修复或主要结构改进；P3 为低风险清理。
- 文件位置以函数名为准，重构后的行号可能变化。
- 只修改任务所需代码，保持现有功能、保存时机、对象生命周期和界面行为。
- 拆分模块时一并迁移其控件与状态，不通过传入 `MainWindow*`、增加 `friend` 或暴露全部成员来维持原有耦合。
- 新增源码时更新 `src/CMakeLists.txt`；格式化修改的 C++ 文件。迁移 Qt `tr()` 调用时保留原翻译上下文，或同步更新 `src/translations/qsnote_en_US.ts`。
- 当前项目没有配置自动化测试套件。行为修复先记录复现步骤，能隔离的逻辑补充有意义的回归验证；不要为纯代码搬迁搭建复杂测试框架。
- 每个任务运行与改动相称的构建和验证。构建输出保存到日志，完成后检查最终结果；不持续轮询大量日志。

## 第一阶段：行为修复与小范围去重

### T01 [P1] 正文读取失败时阻止后续覆盖

- [x] 完成
- 依赖：无。
- 位置：`src/core/NotebookStore.cpp` 的 `loadNotebook()`、`syncNotebook()`；`MainWindow` 的 `storageReady_` 保护。
- 问题：正文打开失败后仍以空内容加载成功；站点保存不要求 `contentDirty`，后续写入可能覆盖原正文。
- 工作：检查正文打开及读取错误并向上传递，确保读取失败不会被当作正常空正文；沿用现有保存保护，避免扩大到无关存储重构。
- 验收：不可读正文触发明确错误，后续保存不覆盖原文；合法空文件可以正常加载；普通、站点、电子书和待办笔记本的正常加载保存不受影响。
- 验证记录：修复前 `tests/notebook-loading.cpp` 在不可读正文用例失败；修复后 `python3 tests/run-regression.py tests/notebook-loading.cpp` 通过。覆盖四种笔记本的正常读写、空正文、权限拒绝、读取错误、文件缺失，并实际调用 MainWindow 保存，确认失败后正文、索引和全局登记文件不变。`cmake --build build --parallel 4` 通过（clang-tidy 开启）。复现：将索引引用的正文设为不可读，重启并尝试保存；预期提示具体正文路径并阻止写入。

### T02 [P2] 统一 PlantUML 语言别名

- [x] 完成
- 依赖：无。
- 位置：`src/ui/InplacePreview.cpp`、`src/resources/web/article.js`、`src/resources/web/article-export.js`、`src/core/ArticleRenderer.cpp`。
- 问题：就地预览支持 `plantuml` 和 `puml`，阅读预览与导出仅识别 `plantuml`。
- 工作：统一语言别名判断，让阅读预览与导出共用 JavaScript 判断逻辑；同步检查 C++ 侧的分派与缓存绕过条件，避免别名支持遗漏在其他入口。
- 验收：两种语言标记在就地预览、阅读预览、站点生成和电子书导出中都能渲染；非法图表仍有错误反馈；其他图表类型不受影响。
- 验证记录：修复前 `tests/plantuml-aliases.cpp` 复现 puml 导出未生成图片；修复后 `python3 tests/run-regression.py tests/plantuml-aliases.cpp` 通过，覆盖 plantuml/puml/PUML 就地与阅读预览、站点图片落盘、HTML/EPUB 图片、缓存绕过、非法图表错误反馈，以及 Mermaid/Graphviz/普通代码。C++ 就地分派与缩放已支持两个别名，保持原实现。`cmake --build build --parallel 4` 通过。

### T03 [P3] 消除重复刷新调用

- [x] 完成
- 依赖：无。
- 位置：`src/ui/MainWindow.cpp` 的构造函数、`closeNotebook()`、`refreshNotebooks()`、`createTodo()`、`refreshTodoCategories()`、`refreshTodoList()`。
- 工作：清理 `refreshNotebooks()` 后重复调用 `refreshEditor()` 的路径；让创建待办后的分类和列表刷新传递选中 ID，避免重复重建列表。
- 验收：启动恢复、关闭笔记本和创建待办后的界面正确；新待办仍被选中且定位到正确分页；大纲、树节点选择和折叠状态不回退。
- 验证记录：`cmake --build build --parallel 4` 通过；工作区回归（保存为 `tests/workspace-state.cpp`，通过 `tests/run-regression.py` 运行）验证启动标签与大纲恢复、树选中与非活动文件夹折叠状态、45 项待办中新建事项跨页定位及选中、清空搜索后详情同步、关闭待办笔记本恢复 Markdown 标签。创建时阻断重置搜索/状态触发的中间刷新，由分类刷新统一传入选中 ID。

### T04 [P3] 清理派生状态与闲置接口

- [x] 完成
- 依赖：无。
- 位置：`src/ui/MainWindow.h/.cpp`、`src/ui/MarkdownPreview.h/.cpp`。
- 工作：用 `viewMode_ == ViewMode::Preview` 替代 `previewModeEnabled_`；重新核对仓库引用后，移除未使用的 `renderedRevision()` 访问器及无连接者的 `renderFinished` 信号与发射代码。
- 边界：保留内部 `renderedRevision_`，它仍用于渲染同步校验；不删除其他未经确认的代码。
- 验收：编辑、预览、分屏切换及 Ctrl+E、大纲跳转、状态栏显示正常；异步渲染版本校验保持有效；构建通过。
- 验证记录：仓库搜索确认被删访问器与信号没有调用者/连接者；保留内部 renderedRevision_ 及异步校验。`cmake --build build --parallel 4` 与 `python3 tests/run-regression.py tests/workspace-state.cpp` 通过，覆盖三种模式显示、Ctrl+E 切换、大纲跳转、状态栏及过期/当前渲染版本回调。

### T05 [P2] 合并重复的原子写文件实现

- [x] 完成
- 依赖：T01。
- 位置：`src/core/NotebookStore.cpp`、`src/core/StaticSite.cpp` 中的 `writeFile()`。
- 工作：提取共享函数，统一创建父目录、跳过内容相同文件、完整写入长度检查、原子提交和错误返回。
- 边界：不强行合并电子书临时文件写入或站点输出路径冲突检查，它们具有不同语义。
- 验收：相同内容不改变文件时间戳；内容变更正确落盘；写入失败返回错误；笔记索引、站点文章和默认页面保存正常。
- 验证记录：提取 AtomicFile::write，统一父目录、读取无错误才比较内容、完整长度检查、QSaveFile 原子提交及路径错误。`cmake --build build --parallel 4`、`tests/atomic-file.cpp`、`tests/notebook-loading.cpp` 均通过（用 `python3 tests/run-regression.py` 运行）；覆盖固定时间戳不变、修改落盘、父路径失败、目录不能被误认为空文件、目录不可写时原文件保留、站点索引/文章/四个默认页面及四类笔记本读写。

### T06 [P2] 合并 KaTeX 字体内嵌处理

- [x] 完成
- 依赖：无。
- 位置：`src/ui/InplacePreview.cpp`、`src/core/ArticleRenderer.cpp` 中的 `mathStylesheet()`。
- 工作：提取共享 CSS 与 WOFF2 字体内嵌函数，统一资源读取失败处理；让两种调用方根据各自流程反馈错误。
- 验收：行内公式、块公式在就地预览与导出中显示正常；生成结果无需外部字体；资源读取失败不被静默处理为有效字体数据。
- 验证记录：共享 embeddedMathStylesheet 只缓存完整结果；读取错误包含资源路径，导出返回失败，就地预览沿既有 rendered 错误通道反馈。`cmake --build build --parallel 4` 和 `python3 tests/run-regression.py tests/math-stylesheet.cpp` 通过；用临时 Qt 资源复现 CSS/字体缺失，验证无部分 CSS 返回，所有 WOFF2 均内嵌且无外部字体引用，行内/块/围栏公式在导出与就地预览中均生成图片。

## 第二阶段：收拢核心业务操作

### T07 [P2] 收拢笔记与待办删除操作

- [x] 完成
- 依赖：T01。
- 位置：`MainWindow::deleteTodo()`、`clearCompletedTodos()`、`showTreeMenu()` 的删除回调；`NotebookStore::removeNoteFiles()`。
- 工作：将删除目标及子项的计算、文件与内存数据删除、保存结果处理收拢到核心层；返回受影响的 ID，供 UI 清理标签和编辑器。先明确失败时哪些修改已经生效，不引入通用命令框架。
- 边界：UI 保留确认对话框和结果展示；清空已完成事项仍覆盖当前笔记本全部分类和分页，不受搜索限制。
- 验收：普通文件夹、电子书父章节、站点文章、单个待办和已完成待办批量删除均正确；关联标签关闭；无关笔记保留；失败不会被展示为成功。
- 验证记录：核心 removeNotes/removeCompletedTodos 计算子项、移除内存记录、保存索引后清理文件，并返回受影响 ID、保存状态及错误。保存失败时内存删除保留、原文件不删；清理失败时索引已更新、残留文件路径明确提示。UI 统一关闭相关标签并保留外部文档保存与编辑器同步。`cmake --build build --parallel 4`、`tests/notebook-removal.cpp`、`tests/workspace-state.cpp` 通过；覆盖四类笔记本、父子孙与资源、47 项跨分类批量删除、索引不可写、正文目录不可写、实际 UI 的搜索/分类筛选下批量清空、单项删除、读取失败保护及关联标签关闭。

### T08 [P2] 收拢分类维护规则

- [x] 完成
- 依赖：T01。
- 位置：待办分类方法、`MainWindow::siteCategories()`、`showTreeMenu()` 和 `showSiteConfig()` 中的站点分类操作。
- 工作：将分类收集、新建、重命名、删除后迁移规则收拢到核心层，让多个 UI 入口调用同一业务操作。
- 边界：保留站点 `default` 与待办“默认”“全部”的不同语义；不为表面相似的代码引入复杂通用分类框架。
- 验收：重名校验一致；默认分类保护有效；重命名及删除同步更新文章或事项；分类移动后的正文和图片路径有效；筛选及选择状态正确。
- 验证记录：NotebookCategories 集中分类收集、创建/重命名重名校验、默认保护和删除迁移；树菜单、文章表单、站点配置及待办共用规则，保存时机仍由原入口控制。`cmake --build build --parallel 4` 与 `python3 tests/run-regression.py tests/notebook-categories.cpp` 通过；覆盖配置/文章隐式分类去重、默认与中英文保留名、空名/重名/不存在分类、两种类型重命名和删除后正文/图片路径及重新加载，以及实际待办筛选和站点配置的选中状态。

### T09 [P2] 收拢章节移动与层级调整

- [x] 完成
- 依赖：T01。
- 位置：`MainWindow::showTreeMenu()` 中电子书上移、下移、缩进、提升层级的回调。
- 工作：将兄弟节点计算、排序与父级修改提取为核心操作；UI 负责菜单可用状态和刷新。
- 验收：首尾章节移动边界正确；缩进和提升后顺序、父子关系、磁盘路径、自动编号及导出顺序一致；子章节和图片资源不丢失。
- 验证记录：ChapterOperations 提供菜单可用性与移动操作，集中兄弟索引、排序和父级修改；UI 继续按原时机保存刷新。`cmake --build build --parallel 4`、`python3 tests/run-regression.py tests/chapter-operations.cpp` 通过；覆盖首尾/根节点边界、穿插子节点时兄弟排序、缩进/提升后的正文与子章节路径、两级图片资源、自动编号、HTML 导出顺序、重新加载，以及子项存储顺序早于父项的提升和实际菜单触发。

## 第三阶段：逐步拆分 MainWindow

### T10 [P2] 提取文章属性对话框并复用新建表单

- [x] 完成
- 依赖：T08。
- 位置：`MainWindow::createNote()` 的站点分支和 `showTreeMenu()` 中的文章属性回调。
- 工作：提取 `PostPropertiesDialog`，接收初始数据，接受后返回表单结果；新建与编辑入口分别负责创建和更新。
- 边界：保留标题、分类、标签、日期、Slug、草稿等默认值。SEO 关键词当前仅在属性编辑中出现，先保留该差异，不顺带改变功能。
- 验收：两个入口共用字段构建与取值逻辑；取消不修改数据；创建、编辑、保存及重新打开后的元数据正确。
- 验证记录：PostPropertiesDialog 共用控件和取值，输入初始标题/元数据/分类，接受后由各入口创建或更新；原 MainWindow 翻译上下文保留，未知元数据键保留。`cmake --build build --parallel 4`、`python3 tests/run-regression.py tests/post-properties.cpp` 通过；实际新建/属性菜单验证默认值、取消无修改、表单字段保存重开、SEO 仅编辑可见、英文翻译。

### T11 [P2] 提取应用设置对话框

- [x] 完成
- 依赖：无。
- 位置：`MainWindow::showSettings()`、匿名命名空间中的 `OverlayDialog`，以及 `MarkdownEditor::createSettingsPage()`。
- 工作：提取 `SettingsDialog`；按实际复用需要移出 `OverlayDialog`；主窗口保留设置生效后的窗口协调，更新检查通过明确接口连接。
- 边界：复用已有编辑器设置页面；保留无打开笔记时可设置编辑器的能力；保留语言重启生效、保存/取消等行为。
- 验收：UI 字体、字号、语言、托盘、启动更新选项及编辑器设置正常；取消不应用表单修改；英文界面和翻译上下文正确。
- 验证记录：SettingsDialog 拥有表单及保存/取消逻辑，复用 MarkdownEditor 设置页，空工作区创建对话框自有隐藏编辑器；更新请求通过信号连接。OverlayDialog 独立复用，共享布局辅助函数迁入 UiStyle；主窗口协调样式与托盘。保留 MainWindow 翻译上下文。`cmake --build build --parallel 4`、`tests/settings-dialog.cpp`、`tests/notebook-categories.cpp` 通过；覆盖空/多标签编辑器、保存前及取消不改配置、UI 字体样式、字号/语言/托盘/更新设置、编辑器同步、语言重启生效、更新信号与英文界面。

### T12 [P2] 提取站点配置对话框

- [ ] 完成
- 依赖：T08、T11。
- 位置：`MainWindow::showSiteConfig()`。
- 工作：提取 `SiteConfigDialog`，迁移配置控件、分类与友链界面；通过明确的数据接口及保存请求连接主窗口。
- 边界：保持当前即时修改、延迟保存以及关闭后保存的语义，不擅自改成“确认后统一提交”。
- 验收：基本信息、头像、评论、主题、分类、友链、发布设置和自定义代码均能保存与恢复；字段联动正确；分类操作使用 T08 的核心规则。
- 验证记录：待填写。

### T13 [P2] 提取待办工作区

- [ ] 完成
- 依赖：T03、T07、T08。
- 位置：`buildTodoUi()`、全部待办方法，以及 `buildUi()`、`refreshEditor()`、`showTreeMenu()` 中的待办分支。
- 工作：提取 `TodoWorkspace`，拥有待办分类栏、列表、详情、筛选、排序、分页和选择状态；主窗口只协调笔记本切换、公共搜索入口及布局。
- 边界：使用核心业务操作；保留完成状态修改后延迟刷新列表的处理，避免鼠标事件中直接销毁当前行。
- 验收：新增、编辑、完成、删除、批量清理、分类管理、搜索、排序、跨页定位正常；切换笔记本后状态正确；不影响 Markdown 标签页恢复。
- 验证记录：待填写。

### T14 [P2] 提取站点预览界面与 HTTP 服务

- [ ] 完成
- 依赖：T11。
- 位置：匿名命名空间中的 `SiteServer`、`MainWindow::showSitePreview()`。
- 工作：提取 `SitePreviewServer` 和 `SitePreviewDialog`，分别负责本地文件服务与浏览器界面；主窗口保留生成站点后启动预览的编排。
- 边界：保留 loopback 监听、站点根路径、GET/HEAD、目录重定向及路径边界检查；对话框关闭时释放服务和浏览器。
- 验收：各内置主题及自定义主题可预览；子路径站点、搜索和资源加载正确；关闭再打开预览不残留监听服务；站点目录外文件不能被访问。
- 验证记录：待填写。

### T15 [P2] 提取电子书导出对话框

- [ ] 完成
- 依赖：无。
- 位置：`MainWindow::exportNotebook()` 的电子书分支、`defaultExportDirectory()`。
- 工作：提取 `EbookExportDialog`，迁移范围、格式、目录选择、校验和进度展示；继续调用已有 `EbookExporter`。让站点发布与电子书导出拥有独立入口，主窗口负责分派。
- 边界：保留导出包含未保存编辑内容的快照语义、目录记忆及同名文件追加序号行为。
- 验收：普通笔记本和电子书的整本/当前文档导出正确；HTML、EPUB、PDF 可验证，CHM 在编译器可用时验证；缺少 CHM 编译器时提示正确；站点发布入口不受影响。
- 验证记录：待填写。

### T16 [P2] 提取编辑器工作区与会话状态

- [ ] 完成
- 依赖：T03、T04、T07、T09、T13、T15。
- 位置：`EditorSession`、标签管理、`activateEditor()`、`removeEditor()`、`refreshEditor()`、`setViewMode()`、`refreshOutline()`、编辑工具栏及相关信号连接。
- 工作：提取 `EditorWorkspace`，拥有标签、编辑器生命周期、会话状态、大纲、模式切换、预览定时器及滚动同步；梳理 `save()` 中的数据落盘与编辑器同步边界。
- 边界：外部文件继续保留 BOM、CRLF 等保存行为；各文档保留自己的撤销历史和视图状态；主窗口通过明确接口连接文档、保存及笔记本选择。
- 验收：多标签切换和关闭、撤销重做、编辑/预览/分屏、滚动恢复、大纲跳转、任务复选框、搜索替换、外部文件打开保存、会话恢复全部通过回归；待办工作区不受影响。
- 验证记录：待填写。

## 第四阶段：拆分站点生成器

### T17 [P2] 提取 front matter 解析模块

- [ ] 完成
- 依赖：无。
- 位置：`src/core/SiteGenerator.cpp` 的 `frontMatter()`；对照 `StaticSite::postSource()`、`postBody()` 的职责。
- 工作：提取独立解析函数或 `FrontMatter` 模块，保留当前 JSON 与简单 YAML 支持范围、正文分离及错误返回。
- 边界：不要把“剥离 QSNote 自动生成元数据”和“解析作者 front matter”混为同一种操作；不扩展 YAML 语法支持。
- 验收：无元数据、JSON、简单 YAML、CRLF、无效 JSON、缺失结束分隔符及不支持的 YAML 行都有明确结果；现有站点生成结果保持一致。
- 验证记录：待填写。

### T18 [P2] 提取站点模板与主题呈现逻辑

- [ ] 完成
- 依赖：T17。
- 位置：`src/core/SiteGenerator.cpp` 的 `loadCustomTheme()`、`themeTemplate()`、主题判断和 HTML 拼接方法。
- 工作：先提取 `SiteTemplateRenderer`，负责模板加载、校验、条件块与占位符替换；随后将主题相关呈现从生成流程中隔离，保留生成器负责扫描、路由、正文处理、索引和输出编排。
- 边界：不引入主题插件框架；保留转义规则、自定义模板限制、路径冲突检查及临时目录替换输出的保护。
- 验收：Pure、NexT、Yilia、3-Jekyll、自定义主题的首页、文章、分类、标签、归档和分页正常；搜索索引、资源链接、评论和自定义代码注入保持一致；无效模板及输出冲突仍报错。
- 验证记录：待填写。

## 建议执行顺序

1. 优先完成 T01、T02，修复已发现的行为风险。
2. 完成 T03～T06，清理小范围重复，降低后续迁移噪声。
3. 完成 T07～T09，收拢核心业务规则。
4. 按依赖完成 T10～T15，先拆边界清楚的对话框和待办工作区。
5. 最后实施 T16，处理耦合最高的编辑器会话管理。
6. T17、T18 作为独立批次处理，避免与主窗口重构混在同一次改动中。

最终验收以职责与状态边界清晰、重复实现减少、行为回归通过为准，不以单纯压低 `MainWindow.cpp` 行数为目标。主窗口保留窗口布局、当前笔记本选择、组件连接和应用关闭等协调职责。
