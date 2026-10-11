# 导出电子书

在“电子书”笔记本右上角点击“导出电子书”，选择导出范围、格式和保存目录，然后点击“导出”。

- **整本电子书**：按左侧章节树的顺序导出全部章节，保留嵌套目录。
- **当前文档**：仅导出当前打开的文档，不包含子章节。未打开文档时只提供整本导出。
- **保存目录**：可直接输入，也可点击“浏览”选择。记住上次使用的目录；同名文件自动添加 `-2`、`-3` 等序号，不覆盖已有文件。

导出使用当前编辑内容，包含尚未自动保存的修改。完成后，底部状态栏显示文件路径。取消设置对话框不会创建文件。

| 格式 | 输出 | 依赖 |
| --- | --- | --- |
| EPUB | EPUB 3 文件，含书名、章节目录、正文及图片 | 应用内置 |
| PDF | A4 文档，含目录页和阅读器侧栏书签，章节分页 | 应用内置 Qt WebEngine |
| CHM | 编译后的 HTML Help 文件，含导航目录 | `chmcmd`，Windows 也支持 `hhc` |
| HTML | 单个可离线阅读的网页，图片嵌入文件 | 应用内置 |

PDF 书签按左侧笔记列表的顺序与层级生成，保留分组和嵌套章节，点击后跳转到对应章节。单篇导出只包含该文档的书签。支持书签侧栏的阅读器会按 PDF 的打开设置显示目录。

Markdown 的标题、列表、表格、代码块和链接由项目内置 cmark 解析器转换。章节间的 Markdown 文件链接及标题锚点会映射到导出内容；指向导出范围以外的本地文档或附件的链接保留文字。外部网页链接保留原地址。原始 HTML 使用 cmark 的安全模式处理；公式、Mermaid 等需要 JavaScript 的扩展保留源码，不执行预览脚本。

本地图片相对于文档所在目录解析，也支持编辑器插入的 data URI 图片。HTTP/HTTPS 图片会在导出时下载；所有图片转为 PNG 随文件打包。图片无法读取或下载时报告失败，避免生成缺图的电子书。

## CHM 编译器

Linux（Ubuntu / Debian）：

```bash
sudo apt install fp-utils
chmcmd --help
```

macOS 可安装 Free Pascal 提供的 `chmcmd`。Windows 可使用 Free Pascal 的 `chmcmd`，或 Microsoft HTML Help Workshop 的 `hhc.exe`。将编译器所在目录加入启动 QSNote 时的 `PATH`。

缺少编译器时，导出对话框说明所需组件并禁用 CHM 导出；其他格式不受影响。CHM 编译在临时目录完成，成功后才保存最终 `.chm` 文件。

## 验证

```bash
cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`qsnote_ebook` 检查范围、顺序、中文、图片、目录、EPUB 包结构、PDF 生成、同名文件和错误处理。安装 `chmcmd` 或 `hhc` 后运行真实 CHM 编译测试；若有 `7z` 和 `pdftotext`，还会提取内容核验。UI 测试覆盖格式选择、保存目录、取消和当前文档的最新编辑内容。
