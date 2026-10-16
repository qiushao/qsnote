# qsnote

C++17 / Qt 6 Widgets Markdown 笔记应用，界面参考 [UI 原型](docs/design/ui/index.html)。首次启动为空，不包含示例笔记。

- 三栏布局：笔记本与搜索、多标签编辑 / Markdown 阅读与分屏预览、自动大纲。
- 本地阅读预览支持公式、Mermaid / Flowchart / Graphviz / WaveDrom 图表、脚注和代码高亮，分屏可实时更新并同步滚动。
- 编辑区支持 KaTeX 行内 / 块级公式及 Mermaid、Flowchart、Graphviz、JSON WaveDrom、PlantUML 和 `mathjax` 代码块的就地预览；公式、图表可分别开关，支持缩放、宽度限制与源码自动折叠。PlantUML 和 Graphviz 的 `sfdp` 布局需要本地渲染引擎。
- 阅读区右键可查找、折叠标题、搜索跳转标题及设置配色 / 字体 / 行距 / 正文宽度；聚焦阅读区后 `Ctrl+G` 打开标题跳转，阅读设置保存后对所有笔记生效。
- 分栏可拖动调整，也可用顶栏右侧相邻的两个按钮收起；窄窗口自动收起侧栏，工具栏与文章元数据自动换行。
- 标题栏左侧「设置」提供常用设置、编辑器设置与关于三个卡片分组：可调整语言（跟随系统 / 简体中文 / English）、UI 字体 / 字号、关闭到系统托盘，以及启动时检查 GitHub 更新；关于页可手动检查并打开版本下载页。设置保存后生效，语言需重启；跟随系统时，中文系统显示简体中文，其他系统显示英文。语言切换不修改已有笔记内容和分类数据。系统无可用托盘时正常退出，托盘菜单可显示窗口或保存并退出。
- Markdown 编辑基于内置 VTextEdit，支持语法高亮、折叠、格式操作、交互表格、Vim / VS Code 输入模式；完整 VNote 功能迁移进度见 [迁移清单](docs/vtextedit-migration.md)。
- 图片按钮、剪贴板图片粘贴，以及从文件管理器复制 / 拖入图片均以 PNG 内嵌到 Markdown，随笔记保存，支持多张图片和撤销 / 重做。
- 编辑器选项菜单中的「编辑器设置…」可配置字体、行距、缩进、换行、行号、就地预览和拼写词典；设置同步到已打开的标签并在下次启动时恢复。
- 支持普通笔记本、静态站点、电子书、Todo List。点击左上角 `＋` 新建笔记本，在左栏右键新建笔记、目录、文章或章节。右键菜单可「关闭笔记本」（仅从列表移除，磁盘数据保留）和「打开笔记本…」（选择已有笔记本目录重新加入）。
- Todo List 采用分类、事项列表、详情三栏布局：左侧切换进行中 / 已完成，固定包含全部 / 默认分类，右键可新建分类；中间新建或勾选事项，右侧编辑标题、分类和详情，自动保存。
- 普通笔记支持拖拽整理；电子书章节支持调整顺序与层级。
- 站点包含文章分类 / 标签 / 日期分组、元数据、站点配置与预览。站点支持发布到 GitHub Pages 或通过 SSH SCP 上传，在站点配置中选择发布方式并填写对应设置。
- 电子书可选择导出当前文档或整本书，支持 EPUB、PDF、CHM、HTML，保留目录和图片；可选择保存目录，同名文件自动编号。CHM 需要 `chmcmd` 或 Windows `hhc`，详见 [电子书导出](docs/ebook-export.md)。
- `Ctrl+F` / `Ctrl+H` 查找替换当前笔记，`Ctrl+Shift+F` 搜索当前笔记本，`Ctrl+E` 切换编辑 / 预览，`Ctrl+S` 保存。新建笔记本时选择保存目录：普通笔记本与电子书按目录 / 章节层级创建文件夹，文章保存为 `.md`；静态站点按分类建目录保存文章。全局 `notebooks.json`（Linux 通常为 `~/.local/share/qiushao/qsnote/`）只登记笔记本基本信息，笔记结构存于各笔记本目录的 `_index.json`。

```text
qsnote/
├── CMakeLists.txt          # Qt、C++ 标准及静态检查配置
├── src/
│   ├── main.cpp
│   ├── ui/                # 主窗口、自适应布局
│   ├── core/              # 笔记数据与本地保存
│   └── resources/         # 样式、图标、桌面入口
├── third_party/           # 第三方源码及许可证，无子模块
├── tests/                 # Qt 交互与布局验证
├── scripts/               # 构建和格式化脚本
└── docs/                  # 文档与参考原型
```

构建方法见 [BUILD.md](BUILD.md)。Linux 快速构建与运行：

```bash
./scripts/linux-build.sh
./build/src/qsnote
```

Linux、macOS、Windows 的构建、打包及标签发布步骤见 [构建与发布](BUILD.md)。

源码使用 4 空格缩进，类名为 `CamelCase`，函数和变量为 `camelBack`，私有和受保护成员以 `_` 结尾。执行 `./scripts/format.sh` 格式化源码。
