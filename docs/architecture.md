# Code layout and function ownership

Remini is split by responsibility. Keep a feature's implementation beside its
interface and avoid adding new application code to the repository root.

Related documents:

- [markdown-parsing.md](markdown-parsing.md) — how Markdown is parsed, masked,
  rendered, and synced back to the raw source (read this before changing syntax
  support).
- [build-and-packaging.md](build-and-packaging.md) — requirements, CMake
  options, Catch2 resolution, test commands, and release packaging.
- [keyboard-shortcuts.md](keyboard-shortcuts.md) — editor shortcuts and
  gestures.
- [cross-platform-development.md](cross-platform-development.md) — platform
  support matrix and cross-platform practices.

## Source layout

- `src/app/` — application entry point, main window, view orchestration, startup
  text, theme definitions, and application event filtering.
- `src/markdown/` — Markdown-aware editor, document formatting, syntax
  highlighting, line/block/fragment data, and shared block-rendering utilities.
  - `mktextdocument.{h,cpp}` — the `QTextDocument` subclass that owns the
    rendered document, the raw source document, and the undo stack. It
    orchestrates parsing, rendering, and raw/rendered sync but delegates the
    details to the helpers below.
  - `markdownparser.{h,cpp}` — stateless Markdown parsing (headings, emphasis,
    inline code, links, checkboxes, blockquotes, images) into `FormatData`.
  - `markdownrenderer.{h,cpp}` — stateless rendering primitives (character/
    block formats, symbol hiding/revealing, code-block chrome).
  - `editcommand.{h,cpp}` — undo/redo commands backed by `EditSnapshot` and
    `EditRestore` value types; no pointers back into the editor widget.
  - `edittypes.h` / `selectrange.h` / `formatcollection.h` — small shared
    value types (`EditType`, `SelectRange`) and the format collection.
  - `markdownconnections.{h,cpp}` — single source of truth for connecting and
    disconnecting an editor to/from a document.
- `src/navigation/` — file-tree model and navigation view interactions.
- `src/search/` — background text/file search worker.
- `src/settings/` — settings dialog, Designer form, and settings-specific notes.
- `src/widgets/` — reusable dialogs and controls.
- `src/platform/` — operating-system integration, including global hotkeys.
- `resources/` — Qt resource collection and image assets.
- `CatchTests/` — automated tests.

## Function responsibilities

### Markdown rendering

- `MarkdownParser::identifyFormatData` decides which Markdown formatting
  applies to a block and produces the `FormatData` fragments.
- `MarkdownRenderer::applyMkFormat` applies one fragment's character/block
  format after its range has passed validation. It does not parse Markdown or
  decide which style to use.
- `MkTextDocument` keeps the "when" of rendering (cursor/selection state,
  signal emission, raw-document sync); the renderer keeps the "how".
- `BlockRenderer` validates the document/block relationship, clamps an
  inclusive character range to visible text, and converts it to the
  exclusive-end convention used by `QTextCursor`.
- `BlockSupport` defines the common positional contract; it does not own
  rendering behavior.

### Undo / redo

- `EditSnapshot` is a plain-value snapshot of an edit captured by `MkEdit`
  before it is committed; `EditRestore` is the value returned after an undo
  or redo.
- `EditCommand` depends only on `MkTextDocument`. It no longer holds or writes
  through pointers into the editor widget.
- The undo stack lives in `MkTextDocument`; `MkEdit` pushes snapshots and
  consumes restore results through the three undo signals.

### UI and navigation

- `MainWindow` owns top-level window behavior and application theme changes.
- `ViewsHandler` coordinates editor/navigation views and relays UI events;
  widget-specific behavior belongs in the widget or view that implements it.
- `NavigationProxyModel` handles filesystem-model filtering and file operations.
- `NavigationView` handles tree-view interaction and emits intent signals.

### Settings

- `SettingsDialog` reads and writes the established `QSettings` keys.
  Keep existing key names stable unless a deliberate migration is included.
- Application-wide theme application remains owned by `MainWindow`; settings
  should signal a theme change rather than duplicate global styling logic.

## Guidelines for new code

1. Put new files in the closest responsibility-based module.
2. Prefer names that describe the result or side effect (for example,
   `applyCharacterFormat` rather than a generic `handleFormat`).
3. Keep parsing, range validation, document mutation, and UI orchestration in
   separate functions.
4. Document non-obvious invariants and failure behavior; avoid comments that
   merely restate a line of code.
5. Update both application and test include/source paths when moving a file.
6. Write comments in English.

## Documentation map

| Change | Document to update |
| --- | --- |
| New or changed Markdown syntax | `docs/markdown-parsing.md` (syntax matrix, pipeline, limitations) |
| New module, file, or ownership change | this file, plus the module `README.md` |
| Build, CMake option, or dependency change | `docs/build-and-packaging.md` |
| New shortcut or editing gesture | `docs/keyboard-shortcuts.md` |
| Platform-specific work | `docs/cross-platform-development.md` |
| User-visible feature | `README.md` feature list |

The syntax matrix in `docs/markdown-parsing.md` is the contract: when a
construct starts or stops working, update the matrix and add a regression test
in the same change.
