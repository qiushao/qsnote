# 构建 qsnote

需要 CMake 3.18+、支持 C++17 的编译器及 Qt 6（Core、Gui、Widgets、Network、Svg、Core5Compat、LinguistTools、WebEngineWidgets、WebChannel，及与运行库版本匹配的 Gui 私有开发头文件）。Linux 默认启用 clang-tidy。

## Linux

Ubuntu / Debian 安装依赖：

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-svg-dev qt6-5compat-dev qt6-webengine-dev qt6-webchannel-dev libgl1-mesa-dev clang-tidy clang-format plantuml graphviz default-jre-headless
```

在工程根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/src/qsnote
```

Release 构建可执行 `./scripts/linux-build.sh [版本号]`，默认版本为 `1.0.0`。

安装到自定义目录：

```bash
cmake --install build --prefix "$HOME/.local"
```

## macOS

```bash
brew install cmake qt plantuml graphviz
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
open build/src/qsnote.app
```

## Windows

安装 CMake、Visual Studio 2022 C++ 工具链及 Qt 6 MSVC 套件。在 Qt MaintenanceTool 中为同一 Qt 版本和 MSVC 架构安装 Qt WebEngine、Qt WebChannel、Qt Positioning 和 Qt 5 Compatibility 附加组件；仅安装桌面基础套件不足以构建本项目。在 PowerShell 中执行（路径替换为实际 Qt 安装位置）：

```powershell
$env:Qt6_DIR = "C:/Qt/6.8.3/msvc2022_64"
./scripts/prepare-windows-runtime.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
& "$env:Qt6_DIR/bin/windeployqt.exe" build/src/Release/qsnote.exe
./build/src/Release/qsnote.exe
```

如果 CMake 报 `Failed to find required Qt component "WebEngineWidgets"`，请先补装上述组件，再重新运行配置；CLion 中执行 Reload CMake Project。使用 Ninja 时，还需要在 MSVC Developer PowerShell 中构建，或在 CLion 中选择 Visual Studio 工具链，使编译器能找到 Windows SDK 头文件和库。

安装规则包含应用和 VTextEdit 动态库；分发 macOS / Windows 应用时，需要使用 Qt 的 `macdeployqt` / `windeployqt` 部署运行时。

Windows 的准备脚本下载 `scripts/windows-runtime.json` 中固定版本的 PlantUML 和 Graphviz，并验证 SHA256。Java 使用系统安装，准备脚本及应用均从 PATH 或 `JAVA_HOME/bin` 查找，不下载、不打包 JRE/JDK。下载缓存和展开的运行时位于 `build-runtime/`，不提交到 Git；CMake 配置和编译本身不联网。CLion 首次配置前也需运行一次准备脚本。构建自动将 PlantUML、Graphviz 复制到可执行文件旁的 `runtime/`，安装和 ZIP 打包也保留此目录，以及各组件的许可证和来源记录。可通过 `-DQSNOTE_DIAGRAM_RUNTIME_DIR=...` 指定已准备好的离线运行时目录。

## 安装发布包及图表依赖

Windows：解压发布的 ZIP 后启动 `qsnote.exe`，保留相邻的 `runtime/`。无需另外安装 PlantUML、Graphviz；PlantUML 需要系统 Java 可通过 PATH 中的 `java` 命令或 `JAVA_HOME` 找到。缺少 Java 时会显示明确提示，不影响公式、Mermaid、Flowchart、WaveDrom 和 Graphviz 的渲染。

Ubuntu / Debian：使用 apt 安装本地 DEB（不要只用 `dpkg -i`），由包管理器解析 Qt、PlantUML、Graphviz 和 Java 的依赖：

```bash
sudo apt install ./qsnote_1.0.0_amd64.deb
```

文件名替换为实际下载的 DEB。Linux 构建产物依赖对应发行版的 Qt 包。

macOS：仓库的 `Casks/qsnote.rb` 安装最新 Release 的 Intel DMG，同时由 Homebrew 安装 `plantuml` 和 `graphviz`；Java 由 `plantuml` 的依赖带入：

```bash
brew tap qiushao/qsnote https://github.com/qiushao/qsnote.git
brew install --cask qiushao/qsnote/qsnote
```

上述命令需在 Cask 合入远端、且发布包含 `qsnote-macos-x86_64.dmg` 的 Release 后使用。目前 Cask 限 Intel、macOS 12+，与发布构建对应；它使用 `version :latest`，更新时运行 `brew upgrade --cask --greedy qiushao/qsnote/qsnote`。直接拖入 DMG 不会触发 Homebrew 安装依赖，此时可手动执行 `brew install plantuml graphviz`。应用从 Finder 启动时也会查找 `/opt/homebrew/bin` 和 `/usr/local/bin`。

## GitHub Actions

`.github/workflows/` 下提供 Linux、macOS 和 Windows MSVC 2022 三套独立流程，沿用 `qshell` 的平台和 Qt 6.2.4 配置（Linux 使用 Ubuntu 22.04 系统 Qt 包）。

- 推送到 `master` / `main`、向这些分支提交 PR，或手动运行：构建、打包并上传 Actions artifact。仅修改 `docs/` 或根目录 Markdown 文件时跳过自动构建。
- 推送 `V*` / `v*` 标签（例如 `V1.2.3`）：去掉标签前缀作为应用版本，并将安装包上传到对应 GitHub Release。
- Linux 产物为 `.deb`，CPack 自动检测 Qt 共享库依赖，并显式声明图表引擎依赖；macOS 为包含 Qt 运行时的 Intel `.dmg`，通过 Cask 安装图表引擎；Windows 为包含 Qt、PlantUML 和 Graphviz 的 x64 `.zip`，Java 使用系统安装。
- 构建任务使用只读权限，只有版本标签触发的发布任务具有 `contents: write` 权限。

Linux 本地生成同样的 DEB 包：

```bash
./scripts/linux-build.sh 1.2.3
cpack --config build/CPackConfig.cmake -B build
```

DEB 打包额外需要 `dpkg-dev`。macOS 包未签名或公证。

## 开发配置

- CLion 直接打开根目录 `CMakeLists.txt`，选择 Qt 6 对应的工具链。
- 非系统安装的 Qt 可通过 `-DCMAKE_PREFIX_PATH=/path/to/Qt` 指定。
- Linux 上如需关闭静态检查，配置时传入 `-DQSNOTE_CLANG_TIDY_ENABLE=OFF`。
- 应用版本可用 `-DAPP_VERSION=1.2.3` 覆盖，运行 `qsnote --version` 查看。

## UI 验证

可选测试依赖 Qt 6 Test（Linux 的 `qt6-base-dev` 已包含）：

```bash
cmake -S . -B build -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

站点生成和集成测试不需要 Node.js、npm 或网络连接。

测试使用独立的应用名与测试数据目录，不读取正式笔记；覆盖空白启动、360～1440 像素窗口布局、新建三种笔记本、右键操作、章节层级、多标签编辑、保存重载、搜索、大纲、站点元数据、配置预览和导出内容。测试运行目录生成界面 PNG，便于与原型进行视觉对照。

Markdown 编辑使用 third_party 中的 VTextEdit 源码，阅读 / 分屏预览使用 Qt WebEngine 与本地 VNote 渲染资源，电子书导出使用 cmark 解析器（EPUB、PDF、CHM、HTML，详见 [电子书导出](docs/ebook-export.md)）；完整迁移仍在进行（见 docs/vtextedit-migration.md）。构建不下载依赖、不初始化子模块。静态站点使用内置 C++ 生成器，详见 [静态站点](docs/static-sites.md)；支持通过 GitHub Pages（需要 git）或 SSH SCP（需要 ssh、scp）发布 public 中的文件。

KaTeX（含字体）、Mermaid、Flowchart/Raphaël、Viz.js 和 WaveDrom 已编入 Qt 资源，无需 Node.js、TeX 或在线服务。PlantUML 预览与导出使用本地 Java 和 Graphviz：Windows 使用随包的 JAR、Graphviz 及系统 Java，Linux/macOS 使用包管理器安装的命令。

编辑区 Graphviz 的 `sfdp` 就地预览使用同一套 Graphviz；其他 Graphviz 布局使用内置 Viz.js。`qsnote_diagram` 测试真实生成 PlantUML 时序图、类图及 `sfdp` PNG；Windows 测试清空 PATH 后通过 JAVA_HOME 使用系统 Java，并验证 PATH 查找和缺少 Java 的提示；macOS 测试移除 Homebrew 的 shell 路径，验证 Finder 启动时的渲染器查找。

VTextEdit 的语法高亮、Vim、拼写检查和 cmark 依赖均已放入 `third_party/vtextedit/libs`，版本记录见 `third_party/README.md`。系统 Qt 仍由包管理器或 Qt 安装器提供；不要混用不同 Qt 版本的私有头文件。
