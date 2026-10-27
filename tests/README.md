# 文本编码回归验证

独立编译和运行编码测试（只依赖 Qt Core、Core5Compat）：

```bash
cmake -S tests -B build/encoding-tests -DQSNOTE_TEST_SOURCE_HIGHLIGHTING=OFF
cmake --build build/encoding-tests
ctest --test-dir build/encoding-tests --output-on-failure
```

覆盖 UTF-8、GBK、UTF-16/32 的自动识别和字节往返、BOM、LF/CRLF/CR 及混合换行符的保留、编辑与主动转换、空文件、歧义编码的手动指定、截断输入、二进制 NUL，以及 GBK 无法表示的字符。

界面回归步骤：

1. 打开包含中文的 GBK `.txt` 和 `.md` 文件，确认文字正确，底部显示 GBK；编辑、保存后，用 GBK 重新读取，文字应保持一致。
2. 打开 UTF-8 文件，确认显示 UTF-8；切换标签页，编码显示应跟随当前文件。打开图片或关闭全部标签后，编码按钮应隐藏。
3. 点击编码按钮 → “以指定编码重新打开”。对同时符合 UTF-8 和 GBK 的字节（例如 `C2 A9`）分别选择两种编码，显示应从 `©` 切换为 `漏`，磁盘文件不应被修改。
4. 有未保存修改时尝试重新打开，确认提示先保存，编辑内容仍保留；选择无法解码的编码时，原有内容和编码应保持不变。
5. 点击编码按钮 → “切换保存编码”，从 GBK 切换到 UTF-8 后保存，确认文字不变，磁盘文件可按 UTF-8 正确读取。包含 emoji 的文字切换到 GBK 应被拒绝。
6. 对普通笔记本、电子书和静态站点重复 GBK 保存、关闭、重新打开；站点生成结果中的中文应正确。

换行符回归步骤：

1. 分别打开使用 LF、CRLF、CR 的 `.txt` 和 `.md` 文件，确认状态栏显示对应类型；修改文字、插入新行并保存，原换行符类型应保持不变。
2. 打开包含 `a\r\nb\nc\rd` 的混合文件，确认显示“混合”。直接保存应保持字节不变；仅修改文字应保留每处原换行符，插入新行使用首个换行符类型。
3. 点击状态栏换行符按钮，依次选择 LF、CRLF、CR 并保存，确认整篇文件使用所选换行符，文字、编码、BOM、末尾是否有换行保持不变；关闭重开后显示正确。
4. 切换标签页，换行符显示应跟随当前文件；图片、无活动文件时隐藏按钮。空文件默认为 LF，选定 CRLF 后开始输入并保存，应使用 CRLF。
5. 对普通笔记本、电子书和静态站点重复编辑、保存、重新打开，确认换行符保留；站点添加或移除元数据头时，正文换行符应保持不变。

## 源码高亮回归验证

包含源码高亮测试的构建还需要 Qt Gui、Widgets、Network 和 Test，使用仓库内的语法库，无需下载：

```bash
cmake -S tests -B build/source-tests
cmake --build build/source-tests --parallel 4
ctest --test-dir build/source-tests --output-on-failure
```

测试直接检查编辑器的文本格式，覆盖关键字匹配（含 SQL 不区分大小写匹配）、C/C++、Java、Python、JavaScript、TypeScript/TSX、Shell 的浅色/深色高亮、中文与 emoji 偏移、注释与字符串、编辑与撤销、扩展名别名和纯文本回退。

界面复现与验证：

1. 直接打开 `.cpp` 文件，内容为 `class Widget { public: int value; void run() { this->value = 1; } };`。修复前关键字无法匹配，自定义类和成员也缺少颜色；修复后 `class`、`public`、`Widget`、`run`、`value` 应按类别着色。
2. 添加 `void Widget::run() { this->value = 1; }`，确认函数名及成员访问有颜色。在代码前添加中文、emoji 和换行，高亮应保持对齐。
3. 把代码放进多行注释或字符串，确认其中的类名与函数名不会染成代码颜色；撤销或继续编辑后，高亮应随内容更新。
4. 切换源码标签页和普通 `.txt` 文件，确认纯文本没有语法颜色，源码内容、撤销历史和保存结果不受高亮影响。

类、函数和成员的补充高亮来自语法结构，不做跨文件类型推断或符号绑定；未带对象限定的成员引用等需要语义分析的位置仍可能使用普通文本颜色。
