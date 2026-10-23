# Repository Guidelines

## Project Structure & Module Organization
QSNote is a local-first Markdown desktop application using C++17 and Qt 6.
- `src/main.cpp` starts the application; `src/ui/` contains windows, editors, and previews.
- `src/core/` handles notebook storage, rendering, exports, site generation, and publishing.
- `src/resources/` contains Qt resources, icons, styles, and bundled site themes; `src/translations/` contains application translations.
- `third_party/` contains vendored VTextEdit and dependencies. Preserve upstream licenses and limit changes to necessary fixes.
- `scripts/`, `cmake/`, and `.github/workflows/` support builds and packaging. `examples/site-theme/` demonstrates custom themes.

## Build, Test, and Development Commands
Install CMake 3.18+, a C++17 compiler, Qt 6 development components (including matching Gui private headers), and Linux clang-tidy. See `BUILD.md` for platform dependencies.

Run from the repository root:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug  # Configure
cmake --build build --parallel              # Build
./build/src/qsnote                          # Run on Linux
./scripts/linux-build.sh 1.2.3               # Release build and DEB
```
For packaging on other platforms, use `./scripts/macos-build.sh 1.2.3` or `./scripts/windows-build.ps1 -Version 1.2.3` after following `BUILD.md` setup instructions.

## Coding Style & Naming Conventions
Follow `.clang-format`: four spaces, no tabs, attached braces, and no fixed column limit. Use PascalCase classes and matching filenames, camelCase functions and variables, and trailing underscores for private/protected members. Follow surrounding code.

Format changed C++ files with `clang-format -i path/to/file.cpp`; `./scripts/format.sh` formats all C++ files under `src/`. Linux builds enable `.clang-tidy` checks by default and treat warnings as errors. Update `src/translations/qsnote_en_US.ts` when changing UI text.

## Testing Guidelines
The application currently has no configured automated test suite or coverage threshold. 

Build and manually verify affected workflows, such as editing, preview, notebook persistence, or export. For bug fixes, record reproduction steps and expected results. CI builds and packages Linux, macOS, and Windows releases and checks packaging artifacts.

## Commit & Pull Request Guidelines
Recent commits use short, descriptive Chinese subjects without mandatory prefixes. Keep each commit focused on one change. PRs should explain the problem and resulting behavior, link relevant issues, and list build/manual verification results. Include screenshots for visible UI changes and identify platform-specific effects.
