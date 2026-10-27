# 构建 qsnote

需要 CMake 3.18+、支持 C11/C++17 的编译器及 Qt 6（Core、Gui、Widgets、Network、Concurrent、Svg、Core5Compat、LinguistTools、WebEngineWidgets、WebChannel，及与运行库版本匹配的 Gui 私有开发头文件）。Linux 默认启用 clang-tidy。


## Linux

Ubuntu / Debian 安装依赖：

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-svg-dev qt6-5compat-dev qt6-webengine-dev qt6-webchannel-dev libgl1-mesa-dev clang-tidy clang-format dpkg-dev plantuml graphviz default-jre-headless
```

在工程根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/src/qsnote
```

Release 构建和 DEB 打包执行 `./scripts/linux-build.sh [版本号]`。优先使用传入的版本号；未传入时通过 `git describe --tags --abbrev=0` 获取最近的标签并去掉 `v` / `V` 前缀，没有标签则使用 `1.0.0`。产物位于 `build/qsnote_<版本号>_<架构>.deb`。

安装到系统（默认 `/usr/local`）：

```bash
sudo cmake --install build
```

安装会刷新图标主题目录，并在系统提供 `gtk-update-icon-cache` 时更新图标缓存。升级后请退出并重新启动 QSNote；如果桌面仍显示旧图标，可注销并重新登录桌面会话。

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

构建和打包 DMG（Qt 的 `bin` 目录需要在 PATH 中，也可设置 `Qt6_DIR` 为 Qt 安装前缀）：

```bash
export Qt6_DIR="$(brew --prefix qt)"
./scripts/macos-build.sh 1.2.3
```

脚本使用独立的 `build-macos/`，生成应用图标、安装到临时目录，并通过 `macdeployqt` 收集 Qt、WebEngine 和 VTextEdit 动态库。许可证随 `.app` 保存，启动验证通过后输出 `build-macos/qsnote-macos-<架构>.dmg`。Intel 发布文件名固定为 `qsnote-macos-x86_64.dmg`，供现有 Homebrew Cask 下载；应用内版本来自脚本参数。

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

一键构建并打包 Windows x64 ZIP：

```powershell
$env:Qt6_DIR = "C:/Qt/6.8.3/msvc2022_64"
./scripts/windows-build.ps1 -Version 1.2.3
```

脚本自动运行图表引擎准备步骤，在 `build-windows/` 中构建和安装，部署 Qt / WebEngine，检查必要文件，并在移除 Qt 开发环境 PATH 后验证启动和版本。产物为 `build-windows/qsnote-1.2.3-windows-x64.zip`，包含 VTextEdit、图表运行时、许可证及本安装说明。若系统缺少 Visual C++ 运行库，先安装 Qt 部署工具随包提供的 `vc_redist.x64.exe`，或微软官方 Visual C++ x64 Redistributable。

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

`.github/workflows/` 下提供 Linux、macOS 和 Windows MSVC 2022 三套独立流程，参考 `qshell` 的标签发布方式，并共用上述本地打包脚本。Linux 使用 Ubuntu 24.04 系统 Qt 包；macOS 使用 `macos-15-intel`，Windows 使用 `windows-2022`，两者安装 Qt 6.4.2 及 WebEngine、WebChannel、Positioning、Qt 5 Compatibility 组件。

- 推送到 `master` / `main`、向这些分支提交 PR，或手动运行：构建、打包并上传 Actions artifact。仅修改 `docs/` 或根目录 Markdown 文件时跳过自动构建。
- 推送 `V*` / `v*` 标签（例如 `V1.2.3`）：去掉标签前缀作为应用版本，并将安装包上传到对应 GitHub Release。
- Linux 产物为 `.deb`，CPack 自动检测 Qt 共享库依赖，并显式声明图表引擎依赖；macOS 为包含 Qt 运行时的 Intel `.dmg`，通过 Cask 安装图表引擎；Windows 为包含 Qt、PlantUML 和 Graphviz 的 x64 `.zip`，Java 使用系统安装。
- 构建任务使用只读权限，只有版本标签触发的发布任务具有 `contents: write` 权限。
- CI 保留安装包文件、依赖声明及 Homebrew Cask 检查，不再配置、编译或运行已删除的测试用例；构建、打包或检查失败都会阻止该平台发布。

Linux 本地生成同样的 DEB 包：

```bash
./scripts/linux-build.sh 1.2.3
```

DEB 打包额外需要 `dpkg-dev`。macOS 包未签名或公证。

发布新版本（先提交并推送配置和代码）：

```bash
git tag V1.2.3
git push origin V1.2.3
```

三个工作流全部成功后，对应 GitHub Release 会包含 DEB、DMG、ZIP 三种产物。手动运行工作流只生成 Actions artifact，不发布 Release；Linux 本地脚本未指定版本时优先读取 Git 标签，macOS / Windows 脚本未指定版本时使用 `1.0.0`。

## 开发配置

- CLion 直接打开根目录 `CMakeLists.txt`，选择 Qt 6 对应的工具链。
- 非系统安装的 Qt 可通过 `-DCMAKE_PREFIX_PATH=/path/to/Qt` 指定。
- Linux 上如需关闭静态检查，配置时传入 `-DQSNOTE_CLANG_TIDY_ENABLE=OFF`。
- 应用版本可用 `-DAPP_VERSION=1.2.3` 覆盖，运行 `qsnote --version` 查看。

应用以中文源码文案为基础，英文翻译位于 `src/translations/qsnote_en_US.ts`。修改界面文案后，可运行 `lupdate src/ui src/core -no-obsolete -locations relative -ts src/translations/qsnote_en_US.ts` 更新条目，并补齐英文翻译。CMake 使用 Qt LinguistTools 编译翻译，随可执行文件内嵌英文、Qt 标准控件中文及 VTextEdit 中文资源，发布时无需另行复制应用翻译文件。
