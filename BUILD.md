# 构建 qsnote

需要 CMake 3.18+、支持 C++17 的编译器及 Qt 6（Core、Gui、Widgets）。Linux 默认启用 clang-tidy。

## Linux

Ubuntu / Debian 安装依赖：

```bash
sudo apt install build-essential cmake qt6-base-dev libgl1-mesa-dev clang-tidy clang-format
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

安装规则仅安装应用本身；分发 macOS / Windows 应用时，需要使用 Qt 的 `macdeployqt` / `windeployqt` 部署运行时。

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
- 当前骨架没有业务测试；可用干净构建、`--help` / `--version` 和主窗口启动验证。
