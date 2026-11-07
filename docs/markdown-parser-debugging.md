# Markdown parser debugging guide

This note is a short companion to `docs/markdown-parsing.md`. It focuses on the practical workflow for understanding and changing the parser without breaking the editor's raw-source model.

## 1. Mental model

Remini does not keep a single live Markdown string and then style it in place. Instead, it keeps two versions of the document:

- The rendered document: what the user sees in the editor.
- The raw document: the original Markdown source kept in `MkTextDocument::rawDocument`.

The parser never mutates text. It only records which source characters should be hidden or replaced while the block is rendered. This is why the editor can show a cleaned-up view while still saving the exact original Markdown.

At a high level:

1. `setPlainText(source)` populates the raw document.
2. `MkTextDocument::identifyUserData()` classifies multi-block constructs such as code fences, tables, and horizontal rules.
3. `MarkdownParser::identifyFormatData()` scans each source line and produces a `FormatData` object.
4. `MarkdownRenderer` applies text formatting, hides masked characters, and inserts images.
5. The block is either shown with markers or hidden with a cleaned-up view depending on the cursor state.

## 2. Where the parser logic lives

The important files are:

- `src/markdown/markdownparser.{h,cpp}` — per-line syntax analysis.
- `src/markdown/formatdata.{h,cpp}` — fragment list, mask, link/image metadata.
- `src/markdown/markdownrenderer.{h,cpp}` — converting parsed fragments into visible formatting.
- `src/markdown/mktextdocument.{h,cpp}` — owning document logic, raw/save sync, block re-rendering.
- `src/markdown/tabledata.{h,cpp}` — table layout and row metadata.
- `src/markdown/blockdata.{h,cpp}` — fenced code block user data.

If a parser bug shows up in the editor, the first place to inspect is usually the parser helper that produced the incorrect fragment or mask.

## 3. The two rules that matter most

### Rule 1: the mask must stay in source coordinates

`FormatData::mask` is indexed by the original block length. Every bit is a source offset. The editor hides characters by looking up the source offset and then deleting the corresponding visible character after the render pass.

This means:

- `mask.size()` must always equal the source block length.
- A replacement like `|` → tab in a table must still consume exactly one rendered character.
- Any new parser feature must preserve offset math for cursor remapping.

If a block's rendered cursor jumps to the wrong place after a hide/show cycle, the bug is usually in the mask or in collapsed-image accounting.

### Rule 2: raw text must survive round-tripping

The rendered document is not the saved document. `rawDocument` is the source of truth. If a block contains an inline image, the visible text may be a single object character, but the raw source still contains the original Markdown expression.

This is why helper functions like `restoreImageMarkdown()` and `FormatData::collapsedCharsBefore()` exist: raw-to-rendered conversion is lossy by design, so the system must reassemble the original literal source when needed.

## 4. Parser flow in practice

### Step 1: classify the line

`MarkdownParser::identifyFormatData()` runs through a fixed sequence of checks:

1. Block quote prefix detection
2. ATX heading detection
3. Inline code spans
4. Escaped characters
5. Plain links
6. Autolinks
7. Emphasis/strong emphasis
8. Checkboxes and angle-form links
9. Images
10. Hidden fragments for click targets and link titles

This ordering matters. For example, inline code must be protected before emphasis scanning so text such as `` `**not bold**` `` stays literal.

### Step 2: generate fragments

Every fragment is a source span plus a semantic meaning. For example:

- bold / italic / strikethrough
- heading levels
- link title / URL
- code span
- image
- blockquote
- checkbox glyph

Fragments are later converted into `QTextCharFormat` or `QTextImageFormat` values during rendering.

### Step 3: render, hide, and remap

When the cursor leaves a block, the text is collapsed to its visible form:

- markers are made invisible,
- image markdown is replaced by a single object character,
- click targets are registered,
- source offsets are mapped back to the rendered text.

When the cursor enters the block again, the raw source is restored and the block is re-parsed before it is rendered in edit mode.

## 5. Debugging strategy

When a Markdown case fails, use this workflow:

### A. Reproduce in a failing test

Most parser regressions should be captured in `CatchTests` before modifying the parser. The relevant test groups include:

```sh
./build/CatchTests/CatchTests "[MarkdownParser]"
./build/CatchTests/CatchTests "[MkTextDocument]"
./build/CatchTests/CatchTests "[MkEdit]"
```

Write one test for the valid case and one test for the nearest invalid case. This catches both the intended feature and common edge cases.

### B. Check the raw source and the rendered source together

A common mistake is to look only at the visible text. Always compare:

- the raw source block,
- the rendered block after hiding,
- the final `FormatData` mask.

If the rendered string is wrong, inspect the fragment list and mask. If the raw source is wrong, inspect the sync path that writes the document back.

### C. Verify protected ranges

Any syntax that should be ignored by later passes must be protected first. The most common bugs happen when:

- inline code is not protected before emphasis,
- escaped characters are not masked before link parsing,
- image syntax is still considered during table detection,
- table separators are processed as normal text before the table parser checks them.

### D. Check cursor remapping after hide/show

The most subtle bugs are not visible formatting mistakes but broken caret behavior. If a block shows the wrong cursor position after toggling focus or after editing, inspect:

- `FormatData::getCalculatedCursorPos()`
- `collapsedCharsBefore()`
- image-related offset adjustments
- table row remapping

## 6. Common gotchas

### Inline emphasis and code

The parser protects code spans before emphasis detection. Without that, `` `**bold**` `` is mistakenly parsed as bold inside code.

### Escaped punctuation

Backslashes are only meaningful before selected delimiter characters. A line like `\*x*` should render as a literal `` and then emphasis on `x`; a literal `\# heading` should not become a heading.

### Link titles and URLs

The parser intentionally handles `[title](<url>)` as the preferred form. It allows spaces and parentheses in the destination, which is a practical fit for local file URLs and Windows paths. The plain form `[title](url)` is still supported, but the angle-bracket form is more robust.

### Tables

Tables are multi-block constructs. Detection happens before rendering. The parser must ensure:

- the header row is followed by a delimiter row,
- the number of cells matches,
- pipes inside code spans are ignored,
- row width and tab stops are preserved while editing.

### Blockquotes and lists

Enter continuation logic depends on previous-line detection. The editor reuses the raw line to continue the same quote or list. If the raw source stops matching the parser assumptions, the editor can lose the effect of quote/list continuation.

## 7. Recommended change checklist

Before turning a parser change into a patch, check each item:

- [ ] The syntax is recognized in `MarkdownParser::identifyFormatData()`.
- [ ] The relevant fragment symbol is mapped in `MarkdownRenderer::applyMkFormat()`.
- [ ] The parser uses source-backed ranges and preserves the `mask.size()` invariant.
- [ ] Cursor positions still remap correctly after hide/show.
- [ ] The raw document still round-trips exactly after a render cycle.
- [ ] There is a regression test for both the valid case and the nearest invalid case.

## 8. Best files to read next

If you are touching parser behavior, read in this order:

1. `docs/markdown-parsing.md` — full architecture and supported syntax rules.
2. `src/markdown/markdownparser.cpp` — actual scanning order and edge-case logic.
3. `src/markdown/formatdata.cpp` — mask and offset bookkeeping.
4. `src/markdown/markdownrenderer.cpp` — how fragment symbols become visible formatting.
5. `src/markdown/mktextdocument.cpp` — raw/save and render lifecycle.
6. `CatchTests` — executable examples of expected behavior.

## 9. Quick test commands

```sh
# run parser-focused tests
./build/CatchTests/CatchTests "[MarkdownParser]"

# run document-level round-trip tests
./build/CatchTests/CatchTests "[MkTextDocument]"

# run editor/editing regressions
./build/CatchTests/CatchTests "[MkEdit]"
```

This is the fastest loop for parser work because it exercises the actual Markdown behavior without needing to manually drive the UI.
