# Code layout and function ownership

Remini is split by responsibility. Keep a feature's implementation beside its
interface and avoid adding new application code to the repository root.

## Source layout

- `src/app/` — application entry point, main window, view orchestration, startup
  text, theme definitions, and application event filtering.
- `src/markdown/` — Markdown-aware editor, document formatting, syntax
  highlighting, line/block/fragment data, and shared block-rendering utilities.
- `src/navigation/` — file-tree model and navigation view interactions.
- `src/search/` — background text/file search worker.
- `src/settings/` — settings dialog, Designer form, and settings-specific notes.
- `src/widgets/` — reusable dialogs and controls.
- `src/platform/` — operating-system integration, including global hotkeys.
- `resources/` — Qt resource collection and image assets.
- `CatchTests/` — automated tests.

## Function responsibilities

### Markdown rendering

- `MkTextDocument::applyMkFormat` decides which Markdown formatting applies to
  a block and delegates range selection to `BlockRenderer`.
- `BlockRenderer::selectRange` validates the document/block relationship,
  clamps an inclusive character range to visible text, and converts it to the
  exclusive-end convention used by `QTextCursor`.
- `BlockRenderer::applyCharacterFormat` applies a character format only after
  the range has passed validation. It does not parse Markdown or decide which
  style to use.
- `BlockSupport` defines the common positional contract; it does not own
  rendering behavior.

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
