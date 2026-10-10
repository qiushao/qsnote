# 构建 qsnote

需要 CMake 3.18+、支持 C++17 的编译器及 Qt 6（Core、Gui、Widgets、Network、Svg、Core5Compat、LinguistTools、WebEngineWidgets、WebChannel，及与运行库版本匹配的 Gui 私有开发头文件）。Linux 默认启用 clang-tidy。

## Linux

Ubuntu / Debian 安装依赖：

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-svg-dev qt6-5compat-dev qt6-webengine-dev qt6-webchannel-dev libgl1-mesa-dev clang-tidy clang-format
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
brew install cmake qt
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
open build/src/qsnote.app
```

## Windows

安装 CMake、Visual Studio 2022 C++ 工具链及 Qt 6 MSVC 套件，在 PowerShell 中执行（路径替换为实际 Qt 安装位置）：

```powershell
$env:Qt6_DIR = "C:/Qt/6.8.3/msvc2022_64"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
& "$env:Qt6_DIR/bin/windeployqt.exe" build/src/Release/qsnote.exe
./build/src/Release/qsnote.exe
```

安装规则包含应用和 VTextEdit 动态库；分发 macOS / Windows 应用时，需要使用 Qt 的 `macdeployqt` / `windeployqt` 部署运行时。

## GitHub Actions

`.github/workflows/` 下提供 Linux、macOS 和 Windows MSVC 2022 三套独立流程，沿用 `qshell` 的平台和 Qt 6.2.4 配置（Linux 使用 Ubuntu 22.04 系统 Qt 包）。

- 推送到 `master` / `main`、向这些分支提交 PR，或手动运行：构建、打包并上传 Actions artifact。仅修改 `docs/` 或根目录 Markdown 文件时跳过自动构建。
- 推送 `V*` / `v*` 标签（例如 `V1.2.3`）：去掉标签前缀作为应用版本，并将安装包上传到对应 GitHub Release。
- Linux 产物为 `.deb`，依赖由 CPack 自动检测，安装时需要系统提供 Qt；macOS 为包含 Qt 运行时的 Intel `.dmg`；Windows 为包含 Qt 运行时的 x64 `.zip`。
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

测试使用独立的应用名与测试数据目录，不读取正式笔记；覆盖空白启动、360～1440 像素窗口布局、新建三种笔记本、右键操作、章节层级、多标签编辑、保存重载、搜索、大纲、站点元数据、配置预览和导出内容。测试运行目录生成界面 PNG，便于与原型进行视觉对照。

Markdown 编辑使用 third_party 中的 VTextEdit 源码，阅读 / 分屏预览使用 Qt WebEngine 与本地 VNote 渲染资源，站点和电子书 HTML 导出暂用 cmark 解析器；完整迁移仍在进行（见 docs/vtextedit-migration.md）。构建不下载依赖、不初始化子模块。站点发布配置仅保存和导出，不自动上传到托管服务。

VTextEdit 的语法高亮、Vim、拼写检查和 cmark 依赖均已放入 `third_party/vtextedit/libs`，版本记录见 `third_party/README.md`。系统 Qt 仍由包管理器或 Qt 安装器提供；不要混用不同 Qt 版本的私有头文件。
