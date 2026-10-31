# Keyboard shortcuts and editing gestures

All shortcuts below are implemented in `src/markdown/mkedit.cpp`
(`MkEdit::keyPressEvent`, `MkEdit::wheelEvent`) unless noted otherwise. The
Markdown-specific behaviour behind them is described in
[markdown-parsing.md](markdown-parsing.md).

---

## Editing

| Shortcut | Action |
| --- | --- |
| `Enter` / `Return` | New line. Continues a blockquote, list, checkbox, or table row if the previous line starts one. |
| `Ctrl+Enter` | Plain new line. **Does not** continue a `>` quote, list, checkbox, or table. |
| `Tab` | Expands the word before the caret (`check`, `ch`, `uncheck`, `uch`, `link`, `lk`) or inserts four spaces. See below. |
| ``` ` ``` (backtick) | Completes a ``` fence and places the caret inside the new code block. |
| `Ctrl+D` | Duplicates the current line. |
| `Ctrl+S` | "Smart select": expands the selection by one word or one bracket/quote pair. |
| `Ctrl+A` | Selects the whole document (revealing all Markdown markers first, so a copy contains the original syntax). |
| `Ctrl+C` | Copy. |
| `Ctrl+V` | Paste. URLs and Windows file paths are wrapped as Markdown links automatically. |
| `Delete` / `Backspace` | Deletes the selection or the adjacent character; merges lines at block boundaries. |

### Tab expansions

| Typed before `Tab` | Result | Caret |
| --- | --- | --- |
| `check`, `ch` | `- [x]  ` | end |
| `uncheck`, `uch` | `- [ ]  ` | end |
| `link`, `lk` | `[](<>)` | inside the title brackets |
| anything else | four spaces | end |

The keyword is matched against the selected prefix of the current word and is
case-insensitive.

### Memory aid for the two Enter variants

`Enter` is "continue the structure", `Ctrl+Enter` is "break out of the
structure":

```
> quoted text▮
Enter          →  > quoted text
                  > ▮

> quoted text▮
Ctrl+Enter     →  > quoted text
                  ▮
```

The same distinction applies to ordered lists (`Enter` increments the number),
task lists (`Enter` inserts the next empty checkbox), and tables (`Enter` adds a
row with the same number of columns, `Ctrl+Enter` leaves the table).

---

## History

| Shortcut | Action |
| --- | --- |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |

The undo stack belongs to the Markdown document, not to the widget, and keeps
both the rendered text and the raw Markdown in sync (see
[markdown-parsing.md §12](markdown-parsing.md#12-undo--redo-integration)).

---

## View

| Shortcut | Action |
| --- | --- |
| `Ctrl` + mouse wheel | Zoom the editor font (clamped to 7–30 pt, persisted in `QSettings` under `fontsize`). |
| `Alt+J` | Global hotkey: show/hide the main window. Windows and X11 only; no-op on Wayland. Implemented in `src/platform/windowapi.cpp`. |
| `Alt+L` | Reserved: the editor swallows it so the platform can use it for its own menu accelerators. |

---

## Context menu

Right-clicking the editor offers:

| Item | Shortcut shown | Notes |
| --- | --- | --- |
| Copy Block / Table | — | Only offered inside a fenced code block or a table; copies the block body or the whole table source. |
| Undo | `Ctrl+Z` | |
| Redo | `Ctrl+Y` | |
| Copy | `Ctrl+C` | |
| Paste | `Ctrl+V` | |
| Delete | `Del` | |
| Select All | `Ctrl+A` | |
| Disable / Enable Markdown | — | Toggles marker hiding for the whole document (persisted). |
| Disable / Enable line-wrap | — | Toggles `WidgetWidth` / `NoWrap`. |
| Reveal Whole Table on Hover | — | Checkable: switches table hover between the current row and the whole table. |

---

## Navigation

| Gesture | Action |
| --- | --- |
| Arrow keys | Move the caret; crossing a block boundary reveals the newly entered block and re-hides the one that was left. |
| `Shift` + arrows | Extend the selection with the same reveal/hide behaviour. |
| Hover a table row | Reveals that row's Markdown source (configurable: off / row / whole table). |
| Click on `☐` / `☑` | Toggles the checkbox and updates the raw Markdown. |
| Click on a link title | Opens the destination with the platform handler. |
| Click elsewhere | Places the caret and reveals that block's Markdown. |

Arrow-key handling is special-cased (`setPreArrowKeys` / `setPostArrowKeys`)
because hiding a block changes its character count: the column of the caret is
remembered and re-applied after the block is re-rendered, so vertical movement
does not drift.
