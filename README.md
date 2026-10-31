![example workflow](https://github.com/AngryFender/Remini/actions/workflows/c-cpp.yml/badge.svg)
![C++](https://img.shields.io/badge/language-C++-blue.svg)
![Qt 6](https://img.shields.io/badge/framework-Qt_6-green.svg)
![CMake](https://img.shields.io/badge/build-CMake-red.svg)
---
# ![reminiicon](https://github.com/AngryFender/Remini/assets/29480834/b1361084-1532-47bd-b755-f1c375dbc510) Remini 
Remini is an ongoing attempt to create a simple, lightweight real-time Markdown Renderer in Qt Widgets. The primary goal of this app is note taking for programming purposes. Reading and understanding codes and cli-commands are made easy using QSyntaxHighlighter and custom Markdown Renderer. Taking notes and editing are made easy by the real-time cursor detection which either shows the Markdown symbols per line when editing and hides the symbols in other lines.

https://github.com/AngryFender/Remini/assets/29480834/2fc6bc0e-da17-424a-8036-dc9708bac8fe

## Documentation

| Document | Contents |
| --- | --- |
| [docs/markdown-parsing.md](docs/markdown-parsing.md) | How parsing, symbol masking, rendering, and raw-source sync work. Supported syntax matrix and known limitations. **Start here to change syntax support.** |
| [docs/architecture.md](docs/architecture.md) | Source layout, module responsibilities, and where new code belongs. |
| [docs/build-and-packaging.md](docs/build-and-packaging.md) | Requirements, CMake options, automatic Catch2 setup, running tests, and the release packaging workflows. |
| [docs/keyboard-shortcuts.md](docs/keyboard-shortcuts.md) | Editor shortcuts, Tab expansions, `Enter` vs `Ctrl+Enter`, and mouse gestures. |
| [docs/cross-platform-development.md](docs/cross-platform-development.md) | Platform support matrix and cross-platform practices. |

## Features

- [x]  **Lexer from scratch**
- [x]  *Markdown* formatting
- [x]  Syntax highlighting
- [x]  Undo/Redo support
- [x]  Fonts Options
- [x]  Search text/files/folder features
- [x]  Windows-level keyboard shortcuts
- [x]  Blockquotes with nesting, auto-continued on `Enter`
- [x]  Inline images (local paths and `file://` destinations)
- [x]  Links, autolinks, and backslash escapes
- [x]  Automated packages for Windows, Debian, and Arch Linux
- [x]  Tables (GFM pipe tables, hover-to-edit rows, rows added on `Enter`)
- [x]  Light and dark themes that also colour rendered Markdown
- [ ]  Emoji library

### Markdown support at a glance

Supported: ATX headings `#`–`######`, bold `**`/`__`, italic `*`/`_`,
strikethrough `~~`, inline code, ```` ``` ```` fenced code blocks, nested
blockquotes `>`, horizontal rules `---`, task lists `- [ ]`/`- [x]`, links in
both `[title](url)` and `[title](<url with spaces>)` form, autolinks
`<https://…>`, backslash escapes `\*`, inline images `![alt](path)`, and GFM
pipe tables (aligned columns, drawn grid).

Not supported: setext headings, inline HTML, link title attributes
(`[t](url "title")`), and remote (http/https) images. See
[docs/markdown-parsing.md](docs/markdown-parsing.md#13-supported-syntax-matrix)
for the full matrix, verified examples, and limitations.

## Quick start

```sh
# build (tests are on by default; Catch2 is fetched automatically)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# run the app
./build/Remini

# run the tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Application-only build without the test dependency:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel
```

## Download

Tagged releases (`v*`) are built by
[`.github/workflows/package.yml`](.github/workflows/package.yml):

| Platform | Artifact |
| --- | --- |
| Windows | `Remini-<version>-win64.zip` (portable, Qt DLLs included) |
| Debian / Ubuntu | `Remini-<version>-amd64.deb` |
| Arch Linux | `remini-<version>-1-x86_64.pkg.tar.zst` |

![remini3](https://github.com/AngryFender/Remini/assets/29480834/7626110f-b6c6-415a-986d-b18c2f6cc8ba)
