# qsnote

参考 `qshell` 组织的 C++17 / Qt 6 Widgets 工程骨架。目前提供应用入口和空白主窗口。

```text
qsnote/
├── CMakeLists.txt          # Qt、C++ 标准及静态检查配置
├── .clang-format          # 沿用 qshell 的代码格式
├── .clang-tidy            # 沿用 qshell 的静态检查规则
├── .github/workflows/     # Linux、macOS、Windows 构建和发布
├── src/
│   ├── CMakeLists.txt      # 应用目标和安装规则
│   ├── main.cpp
│   ├── ui/                # 窗口和控件
│   ├── core/              # 预留业务逻辑和公共工具目录
│   └── resources/         # 桌面入口及后续资源
├── scripts/               # 构建和格式化脚本
└── docs/                  # 文档
```

构建方法见 [BUILD.md](BUILD.md)。Linux 快速构建与运行：

```bash
./scripts/linux-build.sh
./build/src/qsnote
```

源码使用 4 空格缩进，类名为 `CamelCase`，函数和变量为 `camelBack`，私有和受保护成员以 `_` 结尾。执行 `./scripts/format.sh` 格式化源码。
