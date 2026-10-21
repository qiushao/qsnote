# Repository Guidelines

## Project Structure & Module Organization

QSNote is a local-first Markdown desktop application built with C++17 and Qt 6.

- `src/core/`: notebook storage, rendering, ebook export, site generation/publishing, and runtime discovery.
- `src/ui/`: Qt widgets, Markdown editor, and previews; `src/main.cpp` starts the application.
- `src/resources/`: embedded web assets, site themes, icons, and Qt resource manifests; `src/translations/` contains application translations.
- `tests/`: Qt Test suites for UI, sites, publishing, ebooks, and diagram runtimes.
- `third_party/`: vendored VTextEdit and rendering dependencies; preserve upstream licenses.
- `docs/`, `examples/site-theme/`: feature documentation and a custom-theme example. `scripts/`, `cmake/`, and `packaging/` support builds and distribution.

## Build, Test, and Development Commands

Install CMake 3.18+, a C++17 compiler, and Qt 6 development packages, including matching Gui private headers. See `BUILD.md` for platform dependencies and packaging instructions. Linux enables clang-tidy by default.

Run from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DQSNOTE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/src/qsnote
```

These commands configure with tests, build, run registered tests, and launch the Linux application. For a non-system Qt installation, pass `-DCMAKE_PREFIX_PATH=/path/to/Qt` when configuring.

## Coding Style & Naming Conventions

Follow `.clang-format`: four spaces, no tabs, attached braces, and no column limit. Use PascalCase class/file names, camelCase functions and variables, and trailing underscores for private/protected members, following `.clang-tidy`.

Format changed C++ files with `clang-format -i path/to/file.cpp`; `./scripts/format.sh` formats all C++ files under `src/`. Avoid unrelated formatting changes. Update `src/translations/qsnote_en_US.ts` when changing Chinese UI text.

## Testing Guidelines

Use Qt Test in `tests/<Feature>Test.cpp`, with descriptive camelCase test slots and `_data` providers for parameterized cases. Add regression coverage for bug fixes and register new suites in root `CMakeLists.txt`. No numerical coverage threshold is configured. Diagram integration tests require PlantUML, Graphviz, and Java. Use isolated test data rather than personal notebooks.

## Commit & Pull Request Guidelines

Recent commits use concise, change-focused subjects in Chinese or English, without a mandatory prefix. Keep each commit focused. PRs should explain the behavior change, link relevant issues, report validation commands/results, and include screenshots for visible UI changes. Update affected documentation alongside implementation.
