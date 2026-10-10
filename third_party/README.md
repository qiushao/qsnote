# 第三方源码

本目录直接保存源码，不包含 Git 子模块；配置和构建不访问网络。

| 目录 | 来源 | 固定版本 |
| --- | --- | --- |
| vtextedit | 用户指定的本地 vtextedit 仓库 | `9087ec628f8e2aec93bc742aa0110169826a3b7d` |
| vtextedit/libs/cmark | https://github.com/vnotex/cmark | `6b895915ed3aa9150e244c13a83f7e1e37bb9ceb` |
| vtextedit/libs/hunspell | https://github.com/vnotex/hunspell | `9f9e2eca369d3b5bc254d8804c8dac2914ec6e16` |
| vtextedit/libs/sonnet | https://github.com/vnotex/sonnet | `73202060c41ac5c5e7b2729a3ddff06c20168885` |
| vtextedit/libs/syntax-highlighting | https://github.com/vnotex/syntax-highlighting | `25f68718e9df4413fd62b3855349fd1f9f92de8d` |
| vtextedit/libs/katevi | 随 VTextEdit 源码提供 | 同 VTextEdit |

四个外部库版本取自 VTextEdit 的 gitlink，下载固定提交的源码归档后展开。
保留各库的版权声明及 COPYING / LICENSES 文件；移除了 Git 元数据和
`.gitmodules`。Qt 是系统开发依赖，不在此复制运行库或头文件。

本地适配：

- 顶层 `CMakeLists.txt` 只接入库，不构建上游 demo 和测试；不对第三方运行应用的 clang-tidy 规则。
- VTextEdit 的 libs/CMakeLists.txt 改用局部变量，避免强制覆盖宿主的 CMake 缓存。
- 删除上游 src/CMakeLists.txt 对根构建目录的假设；cmark target 自己提供生成头文件路径。
- `qsnote-translations.qrc` 引用上游中文翻译；语法定义引用上游 demo 中的 syntax.qrc。
- 行距应用改为虚方法，让 Markdown 编辑器使用其布局层实现，避免运行时更改行距向正文撤销栈插入段落格式操作。

上游源码、测试和文档均保留，便于后续对照更新。

## VNote 阅读器资源

`vnote-web/` 直接复制自用户提供的 VNote
`0802f7e6ea1dcaee88e0851898c5f399efbb8310` 的 `src/data/extra/web/js/`。
保留 markdown-it 及扩展、KaTeX 字体与许可证、Mermaid、Flowchart/Raphaël、
WaveDrom、Viz.js、Prism 的原始文件和内嵌版权说明；本地未改动这些 JS 库。
`vnote-web.qrc` 把资源编入程序，运行不依赖 CDN。宿主渲染与通信代码位于
`src/resources/web` 和 `src/ui/MarkdownPreview.*`。
