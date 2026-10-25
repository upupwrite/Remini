# Markdown parsing and rendering

This document describes how Remini turns Markdown source into the rendered text
you see in the editor, how editing operations are derived from that parse, and
how the original source is preserved for saving.

It is the reference for anyone changing syntax support. For the module layout
see [architecture.md](architecture.md); for build and test setup see
[build-and-packaging.md](build-and-packaging.md).

---

## 1. Design goals

Remini is an editor, not a preview pane. Three constraints shape the whole
design:

1. **The document you edit is the rendered document.** There is no split view.
   Formatting markers (`**`, `#`, `` ` ``, …) are *deleted* from the visible
   text of every block that is not being edited.
2. **The original source must survive.** Deleting markers is destructive, so
   the untouched Markdown lives in a second, hidden `QTextDocument`
   (`rawDocument`).
3. **The block under the cursor shows its source.** When the cursor enters a
   block, that block is rebuilt from the raw source so the user can edit real
   Markdown characters.

Everything else — fragment lists, the hidden-character mask, hit-test maps — is
bookkeeping that makes those three rules work.

---

## 2. Two documents, one source of truth

| Document | Type | Contains | Purpose |
| --- | --- | --- | --- |
| Rendered document | `MkTextDocument` (a `QTextDocument`) | Formatted, marker-free text | What the user sees and edits |
| Raw document | `QTextDocument` member `MkTextDocument::rawDocument` | Verbatim Markdown | What gets written to disk |

`rawDocument` is the **source of truth** for the file content.
`MkTextDocument::rawText()` and `rawBlockText(blockNo)` expose it to the editor
without handing out the whole document.

Ownership:

```
MkEdit (QTextEdit)
  └── MkTextDocument (QTextDocument)
        ├── rawDocument (QTextDocument)
        ├── undoStack (QUndoStack of EditCommand)
        └── parser (MarkdownParser instance)
```

Rendering helpers (`MarkdownRenderer`, `BlockRenderer`) are stateless and take
the document or block they operate on.

---

## 3. Pipeline at a glance

```
                        setPlainText(source)
                                 │
                 ┌───────────────┴────────────────┐
                 │                                │
        rendered = source                 rawDocument = source
                 │
        identifyUserData()  ── per block ──► attach user data
                 │
    ┌────────────┴─────────────┬──────────────────────┐
    │                          │                      │
 fenced code fence      horizontal rule          everything else
 (`^ {0,3}```.*$`)         (`^---$`)                  │
    │                          │             MarkdownParser::identifyFormatData
 BlockData                LineData                   │
                                              FormatData (fragments + mask)
                                                       │
                                        cursor leaves the block
                                                       │
                                    MarkdownRenderer: apply formats,
                                    delete masked characters
                                                       │
                                              marker-free rendered text
```

Two entry points decide *which* block gets re-rendered:

| Call | When | Scope |
| --- | --- | --- |
| `applyAllMkDataHandle(blockNo)` | Structure-changing edits (Enter, paste, multi-line delete) | Re-parse every block's user data, then re-render all of them |
| `applyMkSingleBlockHandle(blockNo)` | Single-block text edits | Re-parse and re-render one block |
| `cursorPosChangedHandle(range)` | Cursor/selection moved | Hide the blocks the cursor left, show the block(s) it entered |

---

## 4. Data model

### 4.1 Per-block user data

Every `QTextBlock` in the rendered document carries exactly one
`QTextBlockUserData` subclass:

| User data | Meaning | Set by |
| --- | --- | --- |
| `BlockData` | Part of a fenced code block (`start`, `content`, `end`), plus the fence's first/last block numbers and hidden state | `identifyUserData` |
| `LineData` | A horizontal rule (`---`) | `identifyUserData` |
| `FormatData` | Any block containing inline/block Markdown fragments | `MarkdownParser::identifyFormatData` |

A block with **no** user data has nothing to format (plain text) and is simply
reset to default styling.

Inspection helpers on `MkTextDocument`:

```cpp
for (auto it = doc.checkMarkPosBegin(); it != doc.checkMarkPosEnd(); ++it)   // (block, pos)
for (auto it = doc.linkPosBegin();     it != doc.linkPosEnd();     ++it)     // (block, start, end, title)
```

Both lists are (re)built during rendering and are used for mouse hit-testing.

### 4.2 `FragmentData` and the format symbols

A `FormatData` owns a list of `FragmentData`, each describing **one span of the
source line** and **what it means**:

```cpp
class FragmentData : public BlockSupport
{
public:
    enum FormatSymbol
    {
        BOLD, ITALIC, STRIKETHROUGH,
        CHECK_START, CHECKED_END, UNCHECKED_END,
        HEADING1, HEADING2, HEADING3, HEADING4, HEADING5, HEADING6,
        LINK_TITLE, LINK_URL,
        CODE_SPAN,
        IMAGE,
        BLOCKQUOTE,
    };
};
```

`BlockSupport` only provides `getStart()` / `getEnd()`. Spans use **inclusive**
character offsets within the block. The conversion to `QTextCursor`'s
exclusive-end convention happens in `BlockRenderer::selectRange()` and nowhere
else.

### 4.3 The hidden-character mask

The most important structure is `FormatData::mask`, a `QBitArray` whose size is
exactly the source block length. A set bit means *"this source character is
invisible when the block is rendered"*.

```
source:   *  *  b  o  l  d  *  *
mask:     1  1  0  0  0  0  1  1
rendered: b  o  l  d
```

Because the mask is indexed by **source** offset, it also answers the reverse
question: *given a rendered cursor position, where is it in the source?*

```cpp
int FormatData::getCalculatedCursorPos(int posInBlock);   // rendered → source
```

The parser only *marks* characters. It never mutates text. Deletion happens
later, in the renderer, so a parse can be repeated at any time from the raw
block.

Per-symbol mask rules (from `FormatData::addFormat`):

| Symbol | Fragment span | Masked characters |
| --- | --- | --- |
| `**` / `__` | `start+2 … end` | opening run and closing run |
| `*` / `_` | `start+1 … end` | opening char and closing char |
| `~~` | `start+2 … end` | opening run and closing run |
| `# … ` heading | whole marker prefix | leading spaces, hashes, following spaces |
| closing `###` | — | `addMaskRange(hideStart, size-1)` |
| `- [x]` / `- [ ]` | `start … end` | the 5-character bracket group |
| `[title]` | the title text | `[` and `]` |
| `](<url>)` | — | the entire URL part |
| `` `code` `` | the code text | the backtick runs |
| blockquote `>` | whole block | leading spaces and all markers |
| image `![alt](url)` | whole expression | *(none — the expression is replaced)* |

### 4.4 Link and image metadata

`FormatData` keeps side tables so the renderer and the raw-sync code can recover
payloads that are not visible in the text:

| Map | Key | Value |
| --- | --- | --- |
| `linkUrlMap` / `linkTitleMap` | title start offset | URL / title of `[title](<url>)` |
| `linkMapHidden` / `linkTitleMapHidden` | rendered title offset | same, for hit-testing |
| `imageUrlMap` / `imageAltTextMap` | expression start offset | destination and alt text |

### 4.5 Blockquote depth

`FormatData::blockQuoteDepth` stores how many `>` markers the line had, so the
renderer can indent nested quotes:

```cpp
blockFormat.setLeftMargin(18 * quoteDepth);
```

---

## 5. Parsing order inside one block

`MarkdownParser::identifyFormatData(block)` runs a fixed sequence. Order
matters, because later stages must know about earlier ones:

1. **Blockquote marker** — `blockQuoteMarkerEnd()` consumes up to three leading
   spaces plus one or more `>` markers (each optionally followed by one space)
   and registers the fragment. Result: nesting depth and mask range.
2. **ATX heading** — `insertHeadingData()` handles `#` … `######`, and also
   consumes the optional closing hash sequence.
3. **Inline code spans** — `parseInlineCodeSpans()` runs *before* emphasis so
   that `` `**not bold**` `` stays literal. It returns the content ranges it
   claimed.
4. **Emphasis** — `parseEmphasis()` runs a CommonMark delimiter stack, skipping
   the ranges protected in step 3.
5. **Links and checkboxes** — a single left-to-right scan
   (`composeSymbolCombination`) looks at 3-character windows for
   `- [`, `x] `, ` ] `, `](<`, `(<`, `>)` and at single characters for `[`,
   `]`, `(`, `)`, `<`, `>`.
6. **Images** — `parseImages()` scans the whole line and skips matches that
   start inside a code span from step 3.
7. **Hidden fragments** — `addHiddenFormat()` is called for link titles and for
   checkbox glyph positions, converting source offsets to *rendered* offsets.

> Fragments are appended in source order except for images, which are rendered
> in reverse (see §8.2).

---

## 6. Block-level constructs

### 6.1 Fenced code blocks

Detection is a single regular expression applied to the whole line:

```cpp
regexCodeBlock.setPattern(QStringLiteral("^ {0,3}```.*$"));
```

Behaviour:

- Up to three leading spaces are allowed (CommonMark).
- A fence anywhere else in prose (`` inline ``` ``) does **not** open a block.
- Any text after the opening backticks (an info string) is ignored.
- Every block between two fences gets `BlockData` with status `content`; the
  first and last get `start` / `end`.
- All blocks in the run share the same `startBlock` / `endBlock` numbers so the
  renderer can style the group as a unit.

Rendered appearance (`MarkdownRenderer::showHideCodeBlock` +
`setCodeBlockMargin`):

| Block | Treatment |
| --- | --- |
| start fence | fence line hidden, `left/right = fontSize * 7/4`, `top = fontSize` |
| content | `left = fontSize * 10/3`, `right = fontSize` |
| end fence | fence line hidden, `bottom = fontSize` |

`MkEdit::paintEvent()` draws a rounded rectangle behind the whole group using
`BlockData::getStatus()`, which is why code blocks look like cards.

### 6.2 Horizontal rule

`LineData` is attached to a block matching `^---$`. Rendering hides the text
(`hideSymbols(block, "---")`), and `MkEdit::paintEvent()` draws the rule line.
Verified: `---` renders to an empty line with a drawn separator.

### 6.3 Blockquotes

`MarkdownParser::blockQuoteMarkerEnd()` implements the marker grammar:

```
up to 3 spaces  ( '>'  [ ' ' ] )+
```

Consequences:

| Source | Rendered | Depth |
| --- | --- | --- |
| `> quoted text` | `quoted text` | 1 |
| `>no space` | `no space` | 1 |
| `   > indented` | `indented` | 1 |
| `>> nested` | `nested` | 2 |
| `> > nested` | `nested` | 2 |
| `    > code` | unchanged | 0 (four spaces start a code block) |

Styling: italic, foreground `#65727C`, background `rgba(90,120,150,20)`, and a
left margin of `18 * depth`.

`MarkdownParser::blockQuotePrefix()` returns the exact marker prefix of a line
(`"> "`, `">> "`, `"   > > "`, …). The editor uses it to continue the quote on
Enter — see §11.1.

### 6.4 ATX headings

`insertHeadingData()` accepts `#` through `######` with:

- up to three leading spaces,
- mandatory whitespace (or end of line) after the hashes,
- an optional closing sequence (`## title ##`) that is hidden, but only when it
  is preceded by whitespace.

The heading fragment's span is extended by the renderer to the end of the block
(`end = block.length() - 1`), so the whole line is styled.

Styles (relative to the base font size):

| Level | Size | Weight | Colour | Top / bottom margin |
| --- | --- | --- | --- | --- |
| H1 | `1.65 ×` | ExtraBold | `#315F91` | 12 / 8 |
| H2 | `1.42 ×` | Bold | `#3C6F9F` | 10 / 7 |
| H3 | `1.28 ×` | DemiBold | `#477DA5` | 8 / 6 |
| H4 | `1.28 ×` | DemiBold | `#477DA5` | 6 / 4 |
| H5 | `1.16 ×` | DemiBold | `#4F7188` | 5 / 3 |
| H6 | `1.00 ×` | DemiBold, italic | `#65727C` | 4 / 2 |

Setext headings (`Title` followed by `===` or `---`) are **not** supported:
`---` is a horizontal rule, and `===` is plain text.

### 6.5 Lists and checkboxes (rendering)

Rendering is fragment-based, not list-model based:

| Source | Rendered | Notes |
| --- | --- | --- |
| `- item` | `- item` | bullets are plain text; only Enter-continuation is special |
| `1. item` | `1. item` | same |
| `- [ ] task` | `☐ task` | unchecked box |
| `- [x] task` | `☑ task` | checked box |
| `- [ ] - [ ] ` | `☐☐` | two independent boxes |

The 5-character bracket group `- [x]` / `- [ ]` is masked, and the character
that follows it is replaced by `☑` / `☐` through a *hidden* fragment. Hidden
fragments are stored in rendered coordinates so clicking the glyph can find the
matching source checkbox.

### 6.6 Inline images

`MarkdownParser::parseImages()` is a hand-written scanner (not a regex) that
supports:

- `![alt](path)` — plain destination,
- `![alt](<path with spaces>)` — angle-bracket destination,
- balanced parentheses inside a plain destination,
- an optional `"title"` / `'title'` after the destination,
- backslash escapes (`\!`, `\]`, …) and nested `[...]` in the alt text.

Images are skipped when they start inside an inline code span. The rendered form
is a real `QTextImageFormat` object, so the visible character count changes
(see §8.3).

---

## 7. Inline constructs

### 7.1 Inline code spans

`` `code` `` uses a backtick run of length *n*; the closing run must have the
same length. Backticks are masked and the content gets a fragment with the
sentinel symbol `CODE_SPAN_SYMBOL`:

```cpp
#define CODE_SPAN_SYMBOL "__remini_internal_inline_code_span__"
```

The sentinel is only used to route the fragment through `addFormat()`; it never
has to match user text. A run starting the line with three or more backticks is
treated as a code fence, not a code span.

Rendered style: fixed-pitch `Monospace`, no marker characters. Emphasis inside a
code span is ignored — verified: `` `a*b*` `` → `a*b*`.

### 7.2 Emphasis and strong emphasis

`parseEmphasis()` implements the CommonMark delimiter-stack algorithm:

1. Tokenise the block into runs of `*`, `_`, `~`.
2. Compute `canOpen` / `canClose` per run using the left/right flanking rules,
   with the extra intraword restriction for `_` (so `a_b_c` is left alone).
3. Walk forward; when a closer finds a matching opener on the stack, consume
   `min(remaining)` characters, preferring 2 (`**`, strong) over 1 (`*`,
   emphasis).
4. Apply the "multiple of 3" rule for `*` and `_` only.

Runs fully inside a protected inline-code range are skipped.

Verified results:

| Source | Rendered |
| --- | --- |
| `**bold**` | `bold` |
| `*italic*` | `italic` |
| `__bold__` / `_italic_` | `bold` / `italic` |
| `***both***` | `both` (bold + italic) |
| `**a *b* c**` | `a b c` (nested) |
| `a_b_c` | `a_b_c` (no intraword emphasis) |

### 7.3 Strikethrough

`~~text~~` follows GFM: at least two tildes are required and the multiple-of-3
rule does not apply. A single `~` stays literal (verified: `~x~` → `~x~`).

### 7.4 Links

Remini uses an explicit angle-bracket link form:

```
[title](<destination>)
```

This is deliberate: it lets destinations contain spaces and parentheses without
ambiguity (`[microsoft](<file:///C:\Program Files (x86)\Microsoft>)`).

| Source | Rendered |
| --- | --- |
| `[youtube](<www.youtube.com>)` | `youtube` |
| `[](<www.google.com>)` | *(empty)* |
| `[t](u)` | unchanged — plain-parenthesis links are not recognised |

Rendered style: underlined, coloured `rgb(50,118,254)`. Clicking the title opens
the URL with `QDesktopServices::openUrl` (`pushLinkHandle`).

### 7.5 Checkboxes

Handled by the 3-character window scan: `- [` opens, `x] ` / ` ] ` closes. The
group must be exactly five characters wide. See §6.5 for the rendered result and
§10.3 for the raw-document update when a box is toggled.

---

## 8. Hiding and showing symbols

### 8.1 Hiding (block lost the cursor)

`MkTextDocument::hideMKSymbolsFromPreviousSelectedBlocks()`:

1. Apply every fragment through `MarkdownRenderer::applyMkFormat()` — but only
   if `FormatData::isFormatted()` is `false` (avoid double formatting).
2. Guard: if `block.text().size() != mask.size()`, **skip** mask deletion.
   This protects blocks whose text length no longer matches the parse (for
   example a block containing an image, see §8.3).
3. Delete masked characters **backwards from the end of the block**, moving the
   cursor left over unmasked ones. Working backwards keeps earlier offsets
   valid.
4. Replace checkbox characters and register click targets
   (`applyCheckBoxLinkEffect`).
5. Mark the data hidden so the operation is not repeated.

### 8.2 Showing (block gained the cursor)

`MkTextDocument::showMKSymbolsFromCurrentSelectedBlocks()`:

1. Reset character/block formats (`MarkdownRenderer::resetTextBlockFormat`).
2. Replace the block's text with the **raw** line
   (`rawDocument.findBlockByNumber(n).text()`), restoring all markers.
3. Re-parse the block (`parser.identifyFormatData`) — the old `FormatData` is
   discarded and rebuilt.
4. Apply text fragments (everything except `IMAGE`) in source order.
5. Apply `IMAGE` fragments **in reverse order**, so replacing one expression
   cannot shift the offsets of an earlier one.
6. If the block was hidden before, remap the cursor with
   `FormatData::getCalculatedCursorPos()` so the caret keeps its logical place.

### 8.3 Known interaction: images disable masking in their block

Because step 5 replaces a multi-character expression with a single object
character, the block's text length no longer equals `mask.size()`. The guard in
§8.1 therefore skips mask deletion for that block.

Verified behaviour:

```
source:   **bold** ![a](x.png)
rendered: **bold** ￼        ← bold markers stay visible
```

So an inline image and other hidden syntax do not yet coexist correctly in one
block. Markers around the image remain visible; the image itself renders
correctly. This is a known limitation (see §15).

### 8.4 Disabling Markdown

`setMarkdownHandle(false)` (menu: *Disable Markdown*) clears the hide/show sets
and re-renders everything from source, so the user sees plain Markdown.
`revealAllMkSymbols()` (Ctrl+A while Markdown is on) temporarily reveals the
markers so a full-document copy contains the original syntax.

---

## 9. Rendering: symbol → text format

`MarkdownRenderer::applyMkFormat()` is the only place that maps a
`FragmentData::FormatSymbol` to a `QTextCharFormat` / `QTextBlockFormat`:

| Symbol | Text format | Block format |
| --- | --- | --- |
| `BOLD` | ExtraBold weight | — |
| `ITALIC` | italic | — |
| `STRIKETHROUGH` | strike-out | — |
| `HEADING1..6` | size/weight/colour from §6.4 | top/bottom margins from §6.4 |
| `LINK_TITLE` | underline + link colour | — |
| `CODE_SPAN` | fixed-pitch monospace | — |
| `BLOCKQUOTE` | italic + `#65727C` | left margin `18 × depth`, background tint |
| `IMAGE` | inserts a `QTextImageFormat` with alt text as tooltip | — |
| `CHECKED_END` / `UNCHECKED_END` | — (handled as hidden fragments) | — |

`FormatCollection` (built per render pass with the current font size) owns the
`QTextCharFormat` objects, so zooming the editor rescales headings.

---

## 10. Raw-document synchronisation

### 10.1 Whole document

```cpp
MkTextDocument::saveRawDocumentHandler();
```

Copies the rendered plain text into `rawDocument`. Only safe when the rendered
text still equals the source, i.e. for structural edits that re-render
everything. Used for multi-block edits, paste, Enter, and structural deletes.

### 10.2 Single block

```cpp
MkTextDocument::saveSingleRawBlockHandler(int blockNo);
```

Used when the user edits one block. Rendered text cannot be written back
verbatim (markers were deleted), so the new text is *merged* into the previous
source line. The only construct that needs real reconstruction is an image:
`restoreImageMarkdown()` walks the rendered block, and for every
`ObjectReplacementCharacter` finds the original Markdown expression by matching
destination **and** alt text, then re-inserts it in place.

```
raw before:      Before ![a cat](images/cat.png) after
user edits:      Before a cat after        (image survives as ￼)
raw after sync:  Before ![a cat](images/cat.png) after
```

Matching is by `(url, alt)` rather than by position so that deleting one of
several images does not shift the remaining expressions onto the wrong source
snippet.

### 10.3 Toggling a checkbox

`pushCheckBoxHandle(position)`:

1. Maps the rendered click offset to a source offset by counting mask bits.
2. Rewrites `- [x]` ⇄ `- [ ]` in the raw line.
3. Updates the fragment status and swaps the glyph in the rendered text.
4. Writes the raw line back.

---

## 11. Editing behaviour derived from the parse

### 11.1 Enter and Ctrl+Enter

`MkEdit::keyPressEvent` emits

```cpp
enterKeyPressed(blockNumber, newCursorPos, continueQuote);
```

with `continueQuote == !(modifiers & Qt::CTRL)`. `MkTextDocument::numberListDetect`
then inspects the **previous** line:

| Previous line | Enter inserts |
| --- | --- |
| `> quote` | the same quote prefix, e.g. `> ` |
| `> > nested` | `> > ` (depth preserved) |
| `3. item` | the next number, e.g. `4. ` |
| `- item` | `- ` |
| `- [ ] task` | `- [ ]  ` |
| `- - [ ] task` | `- - [ ]  ` |
| anything else | nothing (plain newline) |

**Ctrl+Enter** bypasses this entirely: `MkEdit::keyPressEvent` inserts a bare
paragraph break and returns early, so a blockquote, list, or checkbox is *not*
continued. Use it to escape a quote or list.

### 11.2 Tab

`Tab` at the start of a word expands a keyword via `autoInsertSymbolHandle`:

| Typed | Replaced with |
| --- | --- |
| `check`, `ch` | `- [x]  ` |
| `uncheck`, `uch` | `- [ ]  ` |
| `link`, `lk` | `[](<>)` with the caret inside the title |
| anything else | four spaces |

### 11.3 Backtick

Typing ``` ``` ``` completes a fence: the editor inserts a closing fence and a
new block, leaving the caret inside the code block (`autoCompleteCodeBlock`).

### 11.4 Paste

`insertFromMimeData` special-cases two shapes of pasted text:

| Pasted text | Inserted |
| --- | --- |
| `http(s)://…`, `ftp://…`, `file://…` | `[](<url>)` with caret in the title |
| `C:\path\to\file` | `[](<file:///C:\path\to\file>)` |
| text containing ``` | inserted verbatim as a code block |

Pasting inside a fenced code block skips link rewriting.

### 11.5 Mouse interaction

`MkEdit::isMouseOnCheckBox()` consults the hit-test lists built during rendering:

- `checkMarkPositions` — `(block, renderedOffset)` of every checkbox glyph;
- `linkPositions` — `(block, start, end, title)` of every link title.

The cursor becomes a pointing hand over those rectangles; clicking a checkbox
toggles it, clicking a link opens it.

---

## 12. Undo / redo integration

An edit is classified into an `EditType` before it is applied:

| `EditType` | Meaning | Undo restores |
| --- | --- | --- |
| `singleEdit` | One block changed | That block's raw text |
| `multiEdit` | Several blocks changed (Enter, paste, Ctrl+D) | Whole raw document |
| `multiDelete` | Multi-block deletion | Whole raw document |
| `checkbox` | Checkbox toggle | Whole raw document |
| `enterPressed` | Reserved for Enter-driven structural edits | Whole raw document |

`MkEdit` captures an `EditSnapshot` (plain values: type, old text, block, scroll
position, selection) and emits `undoStackPushSignal(snapshot)`. `MkTextDocument`
pushes an `EditCommand` onto its `QUndoStack`; the command depends only on the
document.

Undo/redo return an `EditRestore` by value (`success`, `editType`,
`selectRange`), which the editor uses to restore the caret and scroll position.
`EditCommand::redo()` skips the first invocation because
`QUndoStack::push()` already calls it.

Because a rendered block may contain images, `singleEdit` redo stores the raw
block text when the rendered text contains `QChar::ObjectReplacementCharacter`.

---

## 13. Supported syntax matrix

Legend: ✅ fully supported · ⚠️ partial / with caveats · ❌ not supported

| Construct | Syntax | Status | Notes |
| --- | --- | --- | --- |
| Heading | `# … ######` | ✅ | closing hashes optional |
| Setext heading | `Title` / `===` | ❌ | `---` is a horizontal rule |
| Bold | `**x**`, `__x__` | ✅ | |
| Italic | `*x*`, `_x_` | ✅ | no intraword `_` |
| Bold + italic | `***x***` | ✅ | |
| Strikethrough | `~~x~~` | ✅ | GFM rules |
| Inline code | `` `x` `` | ✅ | monospace; protects its content |
| Code block | ```` ```…``` ```` | ✅ | info string ignored |
| Blockquote | `> x` | ✅ | nested (`>>`, `> >`), Enter continues |
| Horizontal rule | `---` | ✅ | drawn line, text hidden |
| Unordered list | `- x` | ⚠️ | no bullet rendering; Enter continues |
| Ordered list | `1. x` | ⚠️ | no auto-numbering on render; Enter increments |
| Task list | `- [ ] x`, `- [x] x` | ✅ | clickable ☐ / ☑ |
| Link | `[t](<url>)` | ✅ | angle brackets required |
| Autolink | `<https://…>` | ❌ | |
| Image | `![alt](url)` | ⚠️ | renders; blocks masking in the same line |
| Inline HTML | `<b>x</b>` | ❌ | shown literally |
| Tables | GFM tables | ❌ | |
| Escapes | `\*`, `\[` | ⚠️ | honoured when scanning images; not for other constructs |
| Emoji | `:smile:` | ❌ | |

---

## 14. Extension guide

### 14.1 Adding an inline construct

1. Add a value to `FragmentData::FormatSymbol` (`formatdata.h`).
2. Detect it in `MarkdownParser::identifyFormatData` (usually a new helper) and
   call `formatData->addFormat(start, end, symbol)` or, if the masking pattern
   is unusual, `addMaskRange()` plus a dedicated `addFormat` branch.
3. Map the symbol to a format in `MarkdownRenderer::applyMkFormat`.
4. If the construct must be clickable, store its payload in a side map in
   `FormatData` and register a hit-test entry in
   `MarkdownRenderer::applyCheckBoxLinkEffect` or a new function.
5. If the construct is destroyed by rendering (like images), teach
   `restoreImageMarkdown`-style logic in `MkTextDocument` how to rebuild the
   source text on save.
6. Add tests (see §16).

### 14.2 Adding a block construct

1. Decide whether it is *line-scoped* (fits `FormatData`) or *multi-line*
   (needs its own `QTextBlockUserData` like `BlockData`).
2. For multi-line constructs, detect it in `MkTextDocument::identifyUserData()`
   and give every participating block the shared range information.
3. Add styling/masking in `MarkdownRenderer`.
4. Extend `MkEdit::paintEvent()` if the construct needs decoration behind the
   text (the code-block card and horizontal rule are drawn there).
5. Extend `MkTextDocument::numberListDetect` if Enter should continue it.

### 14.3 Checklist for any syntax change

- [ ] Parser marks characters; it never edits text.
- [ ] `mask.size()` still equals the source block length.
- [ ] Rendered text for a block *without* the construct is unchanged.
- [ ] Cursor remapping after show/hide is still correct
      (`getCalculatedCursorPos`).
- [ ] The raw document still round-trips: `setPlainText(src)` →
      render → `rawText() == src`.
- [ ] Tests cover valid input **and** nearby invalid input (e.g. `>not a quote`
      used to be rejected; `[t](u)` is still rejected).

---

## 15. Known limitations

1. **Images disable masking in their own block** (§8.3). `**bold** ![img](x)`
   keeps its asterisks visible.
2. **Plain-parenthesis links** `[t](u)` are not recognised; use `[t](<u>)`.
3. **Remote images** are not fetched asynchronously. `QTextDocument` can only
   render local/registered resources; HTTP(S) destinations need a resource
   provider.
4. **Nested blockquotes** are styled by depth but there is no lazy continuation
   paragraph handling (a `>` line followed by a plain line is two blocks).
5. **No setext headings, no tables, no autolinks, no inline HTML, no escapes**
   for constructs other than images.
6. **Line-scoped parsing only.** A construct cannot span two lines (emphasis,
   links, code spans are all per block). Blockquotes and code blocks are handled
   specially at the block level.
7. `MkEdit` keeps `qDebug()` tracing in `preUndoSetup` / `postUndoSetup` /
   `dumpEditState`; useful when debugging, noisy in release logs.

---

## 16. Test map

Tests live in `CatchTests/` and are the executable specification of this
document.

| File | Covers |
| --- | --- |
| `mktextdocument_tests.cpp` | Parsing: headings, emphasis, code spans, links, checkboxes, blockquotes, images, raw round-trip, mask maths |
| `MkEdit_Tests.cpp` | Editing: key handling, Enter/Ctrl+Enter, Tab, paste, undo/redo, caret placement, checkbox clicks |
| `editcommand_tests.cpp` | `EditCommand` in isolation (no editor widget) |
| `NavigationView_Tests.cpp` | File tree behaviour |
| `RecentFilesDialog_Tests.cpp` | Recent-files dialog |

Useful filters:

```sh
# everything parsing-related
./build/CatchTests/CatchTests "[MkTextDocument]"

# editing behaviour
./build/CatchTests/CatchTests "[MkEdit]"

# one case by name
./build/CatchTests/CatchTests "MkTextDocument blockquote*"
```

When adding a test for a new construct, prefer asserting the **rendered** text
(`doc.begin().text()`) *and* the raw round-trip
(`doc.getRawDocument()->toPlainText()`), because the two can diverge (that is
exactly what §8.3 describes).
