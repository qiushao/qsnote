# qsnote

C++17 / Qt 6 Widgets Markdown 笔记应用，界面参考 [UI 原型](docs/design/ui/index.html)。首次启动为空，不包含示例笔记。

- 三栏布局：笔记本与搜索、多标签编辑 / Markdown 阅读与分屏预览、自动大纲。
- 本地阅读预览支持公式、Mermaid / Flowchart / Graphviz / WaveDrom 图表、脚注和代码高亮，分屏可实时更新并同步滚动。
- 分栏可拖动调整，也可用顶栏按钮收起；窄窗口自动收起侧栏，工具栏与文章元数据自动换行。
- Markdown 编辑基于内置 VTextEdit，支持语法高亮、折叠、格式操作、交互表格、Vim / VS Code 输入模式；完整 VNote 功能迁移进度见 [迁移清单](docs/vtextedit-migration.md)。
- 编辑器选项菜单中的「编辑器设置…」可配置字体、行距、缩进、换行、行号、就地预览和拼写词典；设置同步到已打开的标签并在下次启动时恢复。
- 支持普通笔记本、静态站点、电子书。点击左上角 `＋` 新建笔记本，在左栏右键新建笔记、目录、文章或章节。
- 普通笔记支持拖拽整理；电子书章节支持调整顺序与层级。
- 站点包含文章分类 / 标签 / 日期分组、元数据、站点配置与预览。站点和电子书可导出到所选目录下的新子目录。
- `Ctrl+F` / `Ctrl+H` 查找替换当前笔记，`Ctrl+Shift+F` 搜索当前笔记本，`Ctrl+E` 切换编辑 / 预览，`Ctrl+S` 保存。内容自动保存到 Qt 应用数据目录的 `notebooks.json`（Linux 通常为 `~/.local/share/qiushao/qsnote/`）。

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

源码使用 4 空格缩进，类名为 `CamelCase`，函数和变量为 `camelBack`，私有和受保护成员以 `_` 结尾。执行 `./scripts/format.sh` 格式化源码。
