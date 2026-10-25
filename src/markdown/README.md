# Markdown module

This directory owns the Markdown editor, its document model, the parser, the
renderer, and the undo/redo commands.

Full behaviour reference: [../../docs/markdown-parsing.md](../../docs/markdown-parsing.md).
Layout and ownership rules: [../../docs/architecture.md](../../docs/architecture.md).

## Files

| File | Responsibility |
| --- | --- |
| `mkedit.{h,cpp}` | `QTextEdit` subclass: key handling, block painting, context menu, mouse hit-testing, undo snapshot capture. |
| `mktextdocument.{h,cpp}` | `QTextDocument` subclass: owns the rendered document, the raw source document, the undo stack, and the hide/show orchestration. |
| `markdownparser.{h,cpp}` | Stateless parsing of one block into `FormatData` (headings, emphasis, code spans, links, checkboxes, blockquotes, images). |
| `markdownrenderer.{h,cpp}` | Stateless rendering primitives: character/block formats, symbol hiding/revealing, code-block chrome, hit-test registration. |
| `blockrenderer.h` | `BlockRenderSupport` range validation and inclusive→exclusive conversion. The only place that does that conversion. |
| `formatdata.{h,cpp}` | Per-block parse result: fragments, hidden-character mask, link/image metadata, blockquote depth. |
| `blockdata.{h,cpp}` | User data for fenced code blocks (`start` / `content` / `end`). |
| `linedata.{h,cpp}` | User data for horizontal rules. |
| `blocksupport.h` | Common `getStart()` / `getEnd()` contract. |
| `editcommand.{h,cpp}` | Undo/redo commands plus `EditSnapshot` / `EditRestore` value types. |
| `edittypes.h` | `EditType` enum. |
| `selectrange.h` | `SelectRange` / `RawBlockInfo` cursor-state value types. |
| `formatcollection.h` | `QTextCharFormat` set built per render pass from the current font size. |
| `markdownconnections.{h,cpp}` | One place to connect/disconnect an editor and a document. |
| `highlighter.{h,cpp}` | `QSyntaxHighlighter` for code blocks and search matches. |

## Mental model

- The **rendered** document is what the user edits; Markdown markers are deleted
  from every block that does not hold the cursor.
- The **raw** document (`MkTextDocument::rawDocument`) is the source of truth for
  saving.
- The **mask** (`FormatData`) records which source characters are invisible, so
  the parse can be replayed and cursor positions remapped.

## Rules for changes

1. The parser only *marks* characters; it never edits text.
2. Never convert inclusive spans to `QTextCursor` positions outside
   `BlockRenderer`.
3. Keep `mask.size()` equal to the *source* block length.
4. After rendering a block, `rawDocument` must still round-trip the file
   content (except for the documented image/mask interaction).
5. Add a regression test for the valid case **and** the nearest invalid case.

## Tests

```sh
./build/CatchTests/CatchTests "[MkTextDocument]"   # parsing
./build/CatchTests/CatchTests "[MkEdit]"           # editing
./build/CatchTests/CatchTests "[MarkdownParser]"   # parser helpers
./build/CatchTests/CatchTests "[EditCommand]"      # undo/redo in isolation
```
