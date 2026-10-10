# VTextEdit / VNote 编辑器迁移

目标是接入用户指定版本 VNote 的全部 Markdown 编辑器能力。下表区分已经接入的
能力与尚未实现的宿主功能；库能提供某项 API 不等于 QSNote 已经完成该功能。

参考源码：

- `vtextedit/src/include/vtextedit/{vmarkdowneditor,vtexteditor,markdowneditorconfig}.h`
- `vnote/src/widgets/editors/{markdowneditor,previewhelper,markdownviewer,markdownvieweradapter}.cpp`
- `vnote/src/widgets/markdownviewwindow2.cpp`
- `vnote/src/controllers/markdowneditorcontroller.cpp`
- `vnote/src/widgets/dialogs/settings/{markdowneditorpage,texteditorpage}.cpp`
- `vnote/src/data/extra/web/js/`

## 已接入，需持续回归

- VMarkdownEditor 替代 QPlainTextEdit；随库提供 cmark、语法高亮、KateVi、Sonnet、Hunspell 源码。
- 行号、代码高亮、文本折叠、补全、普通 / Vim / VS Code 输入模式入口及输入模式状态栏。
- 标题、粗体、斜体、删除线、高亮、列表、任务、引用、代码、公式等走 VTextEdit 格式命令。
- 链接、图片、指定行列数的表格插入；图片当前以 PNG data URL 内嵌。
- 表格就地预览和单元格编辑、自动表格源码对齐、有序列表编号。
- 查找替换对话框：前后查找、大小写、全词、正则、替换、全部替换。
- 大纲采用 VTextEdit 的标题解析事件，保留标题层级和源码位置。
- 站点和电子书 HTML 导出改用 VTextEdit 的 cmark 解析器；当前与 Web 阅读预览尚未统一。
- 翻译、基础编辑选项、现有笔记本 / 标签 / 自动保存 / 布局行为接入。
- 每个标签保留独立编辑器、撤销记录、选区、光标、滚动位置和编辑 / 阅读 / 分屏模式；后台正文变化按笔记 ID 保存。
- 全局编辑器设置通过 QSettings 保存：输入模式、绝对 / 相对行号、换行方式、光标居中、字体与字号、缩放、Tab、行距、正文宽度、折叠和空白标记。
- Markdown 设置支持图片 / 表格预览开关、预览宽度、预览自动折叠、链接隐藏、表格对齐和列表编号；拼写设置支持开关、语言自动识别和已安装词典选择。
- 设置同步到已打开的标签；取消对话框不改变设置。修改行距使用 Markdown 布局层，不占用正文撤销记录。

## 本轮接入的阅读预览

- Qt WebEngine 阅读和编辑 / 预览分屏，修改后合并更新，按源码行号双向同步滚动。
- 使用 VNote 本地 markdown-it 及扩展：脚注、任务、上下标、Emoji、高亮、目录、标题锚点、元数据、告警容器和图片说明。
- KaTeX 数学公式（单行 `$$…$$` 保持行内放置）、Mermaid、Flowchart、Graphviz / Viz.js、JSON 格式 WaveDrom、Prism 代码高亮。
- 阅读区任务勾选写回源码，支持撤销；图片点击放大；外部网页链接交由系统浏览器打开。
- 渲染带有文档版本标识，旧渲染结果不能覆盖新内容；写回前核对笔记和完整源文本。
- 所有 Web 库资源保存在 `third_party/vnote-web` 并编入 Qt 资源，无 CDN 加载。
- 阅读区查找支持前后查找、大小写和匹配计数，查找前展开折叠章节；标题折叠、全部展开 / 折叠、锚点跳转展开目标祖先已经接入。
- 阅读区右键「阅读设置…」支持浅色 / 深色 / 纸张配色、字体、字号、行距、正文宽度，确认后持久化，取消不改变页面。默认保持原有外观，图表在深色模式中保留白底。
- 阅读区右键「跳转到标题…」或聚焦阅读区后 `Ctrl+G`，支持按标题筛选、回车 / 双击跳转，并展开目标的折叠祖先；文档变化关闭旧导航，防止跳到过期位置。

## 尚未完成，不能据此宣布全部功能迁移完成

- 阅读器完整行为对照：演示与导出，以及其余 Markdown 扩展边界；已实现的查找、折叠、标题跳转和阅读样式仍需与完整 VNote 行为对照。
- MathJax 切换、PlantUML、WaveDrom 的 JavaScript 对象写法，以及所有公式和图表在编辑区的就地预览。
- 图片附件生命周期、本地与远程图片、拖放与剪贴板、图片缩放和编辑、图床、HTML 转 Markdown。
- 其余设置持久化、编辑器与应用主题切换、自定义 Vim 快捷键、聚焦模式和 VNote 打字机模式的完整行为对照（光标居中、阅读区配色已接入）。
- HTML / PDF / 图片 / 演示导出及与阅读器一致的离线资源打包。
- 按 VNote 参考源码逐项补全行为清单，完成跨平台构建、安装产物和实际交互验证。

## 验证

`cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON` 后构建并运行 CTest。
`tests/UiTest.cpp` 已增加多行格式与撤销、查找替换、交互表格写回的宿主集成测试，
原有笔记本、标签、保存、导出、搜索、布局测试继续使用真正的 VTextEdit 控件。
标签会话测试覆盖独立撤销、选区和滚动恢复、视图模式记忆、后台笔记保存，以及关闭 Vim 命令栏后的编辑器释放。
设置测试使用临时配置目录，覆盖取消、磁盘保存、新编辑器恢复、跨标签同步、菜单勾选状态，以及配置应用后正文、选区和撤销 / 重做的保留。
这些测试只证明覆盖的集成行为，不代表上述未完成项已经通过验证。

WebEngine 测试增加实际 DOM / SVG / KaTeX 渲染断言、切换笔记后丢弃旧结果、
主窗口实时分屏、任务写回与撤销、双向滚动、单行 display math 布局断言。
测试使用 offscreen/software 渲染；受限执行沙箱若不能创建 Chromium 通信通道，
应在正常宿主环境运行测试，而不是关闭应用的 Chromium 沙箱。

阅读设置与导航测试覆盖取消、保存、新阅读器恢复、实际 CSS 样式、标题关键词筛选、
快捷键打开、折叠章节内跳转、切换文档关闭旧导航及无标题提示。
