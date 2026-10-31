#include "mktextdocument.h"
#include "blockrenderer.h"
#include "blockstructure.h"
#include "markdownparser.h"
#include "markdownrenderer.h"

#include <QChar>
#include <QColor>
#include <QTextBlockFormat>
#include <QTextImageFormat>

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------
namespace {

// QTextBlock::text() exposes an inline image as U+FFFC, whereas the raw
// document stores its Markdown syntax. Restore each original expression in
// source order so valid destinations with spaces or balanced parentheses
// survive an edit to the rendered block.
QString restoreImageMarkdown(const QTextBlock &renderedBlock,
                             const QString &sourceMarkdown)
{
    const QString renderedText = renderedBlock.text();
    const auto images = MarkdownParser::parseImages(sourceMarkdown);
    QVector<bool> used(images.size(), false);

    QString result;
    result.reserve(renderedText.size());
    for (int offset = 0; offset < renderedText.size(); ++offset)
    {
        const QChar character = renderedText.at(offset);
        if (character != QChar::ObjectReplacementCharacter)
        {
            result.append(character);
            continue;
        }

        // Match each surviving rendered image to its original Markdown by
        // destination and alt-text tooltip. Sequential replacement alone is
        // incorrect when an earlier image is deleted: it would assign that
        // deleted image's Markdown to the next image.
        QTextCursor imageCursor(renderedBlock);
        imageCursor.setPosition(renderedBlock.position() + offset);
        imageCursor.movePosition(QTextCursor::NextCharacter,
                                 QTextCursor::KeepAnchor);
        const QTextImageFormat imageFormat =
            imageCursor.charFormat().toImageFormat();
        const QString imageUrl = imageFormat.name();
        const QString imageAlt = imageFormat.toolTip();

        int matchIndex = -1;
        for (int i = 0; i < images.size(); ++i)
        {
            if (used.at(i) || images.at(i).url != imageUrl)
                continue;
            if (images.at(i).alt == imageAlt)
            {
                matchIndex = i;
                break;
            }
            if (matchIndex < 0)
                matchIndex = i;
        }

        if (matchIndex >= 0)
        {
            used[matchIndex] = true;
            result.append(images.at(matchIndex).source);
        }
        else if (!imageUrl.isEmpty())
        {
            // An image inserted from outside the Markdown source has no
            // original expression to restore. Serialize its URL rather than
            // accidentally consuming another image's source expression.
            result.append(QStringLiteral("![](<") + imageUrl +
                          QStringLiteral(">)"));
        }
        else
        {
            result.append(character);
        }
    }
    return result;
}

}  // namespace

MkTextDocument::MkTextDocument(QObject *parent) : QTextDocument{parent}
{
    // A fence must begin the line (CommonMark allows up to three leading spaces).
    // Matching ``` anywhere in prose incorrectly turns following lines into code.
    regexCodeBlock.setPattern(QStringLiteral("^ {0,3}```.*$"));
    regexHorizontalLine.setPattern(HORIZONTALLINE_SYMBOL);
    regexBulletCheckBox.setPattern("\\s*- - \\[.\\] \\s*");
    regexCheckBox.setPattern("\\s*- \\[.\\] \\s*");
    this->setUndoRedoEnabled(false);
    linkColor.setRgb(51, 102, 204);
    disableMarkdownState = false;
}

void MkTextDocument::setPlainText(const QString &text)
{
    // QTextDocument::setPlainText("") is a valid reset operation. Do not
    // return early: callers use an empty string to clear an existing file.
    QTextDocument::setPlainText(text);
    this->rawDocument.setPlainText(text);
    identifyUserData();
    undoStack.clear();
}

void MkTextDocument::setUndoRedoText(const QString &text)
{
    QTextDocument::clear();
    QTextDocument::setPlainText(text);
    this->rawDocument.setPlainText(text);
}

void MkTextDocument::setUndoRedoText(const int blockNo, const QString &text)
{
    QTextCursor cursor(this);
    cursor.setPosition(findBlockByNumber(blockNo).position());
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(text);

    QTextCursor rawCursor(&rawDocument);
    rawCursor.setPosition(rawDocument.findBlockByNumber(blockNo).position());
    rawCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    rawCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    rawCursor.insertText(text);
}

void MkTextDocument::setUndoEnterPressedText(const int blockNo,
                                             const QString &text)
{
    QTextCursor cursor(this);
    cursor.setPosition(findBlockByNumber(blockNo).position());
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(text);
    cursor.movePosition(QTextCursor::NextBlock);
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.deleteChar();
    cursor.deleteChar();

    QTextCursor rawCursor(&rawDocument);
    rawCursor.setPosition(rawDocument.findBlockByNumber(blockNo).position());
    rawCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    rawCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    rawCursor.insertText(text);
    rawCursor.movePosition(QTextCursor::NextBlock);
    rawCursor.select(QTextCursor::BlockUnderCursor);
    rawCursor.deleteChar();
    rawCursor.deleteChar();
}

void MkTextDocument::clear()
{
    checkMarkPositions.clear();
    linkPositions.clear();
    QTextDocument::clear();
    rawDocument.clear();
}

void MkTextDocument::setFilePath(const QString &filePath)
{
    this->filePath = filePath;
}

void MkTextDocument::setFileName(const QString &fileName)
{
    this->fileName = fileName;
}

void MkTextDocument::setCursorPos(const int blockNo, const int characterNo)
{
    this->blockNo = blockNo;
    this->characterNo = characterNo;
}

int MkTextDocument::getBlockNo() const { return this->blockNo; }

int MkTextDocument::getCharacterNo() const { return this->characterNo; }

QString MkTextDocument::getFilePath() const { return this->filePath; }

QString MkTextDocument::getFileName() const { return this->fileName; }

QTextDocument *MkTextDocument::getRawDocument() { return &rawDocument; }

QString MkTextDocument::rawText() const
{
    return rawDocument.toPlainText();
}

QString MkTextDocument::rawBlockText(int blockNo) const
{
    const QTextBlock block = rawDocument.findBlockByNumber(blockNo);
    return block.isValid() ? block.text() : QString();
}

void MkTextDocument::cursorPosChangedHandle(SelectRange *const range,
                                            const bool readOnly)
{
    if (!range || readOnly)
        return;

    this->selectRange.hasSelection = range->hasSelection;
    if (range->hasSelection)
    {
        this->selectRange.selectionFirstStartBlock =
            qMin(range->selectionFirstStartBlock, range->selectionEndBlock);
        this->selectRange.selectionEndBlock =
            qMax(range->selectionFirstStartBlock, range->selectionEndBlock);
    }
    else
    {
        this->selectRange.currentposInBlock = range->currentposInBlock;
        this->selectRange.currentBlockNo = range->currentBlockNo;
    }

    refreshVisibleBlocks();

    range->currentBlockNo = this->selectRange.currentBlockNo;
    range->currentposInBlock = this->selectRange.currentposInBlock;
}

void MkTextDocument::refreshVisibleBlocks()
{
    if (disableMarkdownState)
        return;

    // Everything is rendered unless the cursor, the selection, or the mouse
    // asks for the source of a block.
    QSet<int> visible;
    if (this->selectRange.hasSelection)
    {
        for (int num = this->selectRange.selectionFirstStartBlock;
             num <= this->selectRange.selectionEndBlock; ++num)
            visible.insert(num);
    }
    else
    {
        visible.insert(this->selectRange.currentBlockNo);
    }

    cursorVisibleBlocks.clear();
    if (this->selectRange.hasSelection)
    {
        for (int num = this->selectRange.selectionFirstStartBlock;
             num <= this->selectRange.selectionEndBlock; ++num)
            cursorVisibleBlocks.insert(num);
    }
    else
    {
        cursorVisibleBlocks.insert(this->selectRange.currentBlockNo);
    }

    if (hoveredBlockNo >= 0)
    {
        auto *table = dynamic_cast<TableData *>(
            this->findBlockByNumber(hoveredBlockNo).userData());
        if (table && hoverWholeTable)
        {
            for (int num = table->getTableStartBlock();
                 num <= table->getTableEndBlock(); ++num)
                visible.insert(num);
        }
        else if (table)
        {
            visible.insert(hoveredBlockNo);
        }
    }

    this->selectRange.showBlocks.clear();
    this->selectRange.hideBlocks.clear();
    for (int num = 0; num < this->blockCount(); ++num)
        this->selectRange.hideBlocks.insert(num);
    for (const int num : visible)
    {
        this->selectRange.hideBlocks.erase(num);
        this->selectRange.showBlocks.insert(num);
    }

    hideMKSymbolsFromPreviousSelectedBlocks(&this->selectRange);
    showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
}

void MkTextDocument::setMarkdownPalette(const MarkdownPalette &newPalette)
{
    palette = newPalette;
}

const MarkdownPalette &MkTextDocument::markdownPalette() const
{
    return palette;
}

void MkTextDocument::setHoveredBlock(const int blockNo)
{
    if (hoveredBlockNo == blockNo)
        return;

    hoveredBlockNo = blockNo;
    refreshVisibleBlocks();
}

void MkTextDocument::setHoverWholeTable(const bool whole)
{
    if (hoverWholeTable == whole)
        return;

    hoverWholeTable = whole;
    refreshVisibleBlocks();
}

bool MkTextDocument::isHoverWholeTable() const { return hoverWholeTable; }

int MkTextDocument::hoveredBlock() const { return hoveredBlockNo; }

void MkTextDocument::removeAllMkDataHandle(int blockNo)
{
    Q_UNUSED(blockNo);

    // clear() resets both the rendered and source documents. Preserve the
    // source first because this operation is only meant to reveal Markdown
    // syntax in the rendered document, not discard the source.
    const QString sourceMarkdown = rawDocument.toPlainText();
    this->clear();
    QTextDocument::setPlainText(sourceMarkdown);
    rawDocument.setPlainText(sourceMarkdown);

    // The document now literally holds the source text, so re-classify the
    // blocks. Structural edits (Enter, paste) read this user data to decide
    // whether to continue a table row, quote, or list.
    identifyUserData();
}

void MkTextDocument::applyAllMkDataHandle(int blockNumber)
{
    QTextBlock block = this->findBlockByNumber(blockNumber);
    block.setUserData(nullptr);

    identifyUserData();

    if (disableMarkdownState)
    {
        for (int num = 0; num < this->blockCount(); num++)
            this->selectRange.showBlocks.insert(num);
        showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
        return;
    }

    this->selectRange.showBlocks.clear();
    this->selectRange.hideBlocks.clear();

    for (int num = 0; num < this->blockCount(); num++)
        this->selectRange.hideBlocks.insert(num);

    this->selectRange.hideBlocks.erase(blockNumber);
    hideMKSymbolsFromPreviousSelectedBlocks(&this->selectRange);

    cursorVisibleBlocks.clear();
    cursorVisibleBlocks.insert(blockNumber);
    this->selectRange.showBlocks.insert(blockNumber);
    showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
}

void MkTextDocument::applyMkSingleBlockHandle(int blockNumber)
{
    QTextBlock block = this->findBlockByNumber(blockNumber);
    QTextBlockUserData *data = block.userData();

    BlockData *blockData = dynamic_cast<BlockData *>(data);
    if (blockData)
        return;

    // A table row keeps its table layout (column stops, row kind) across
    // re-parses; only the inline fragments and the mask are rebuilt.
    if (auto *table = dynamic_cast<TableData *>(data))
    {
        TableData *fresh = table->cloneLayout();
        const int textLength = block.text().size();
        fresh->setMaskSize(textLength);
        block.setUserData(fresh);

        if (fresh->getRowKind() == TableData::Delimiter)
            fresh->addMaskRange(0, textLength - 1);
        else
            parser.identifyFormatData(block, fresh);
    }
    else
    {
        block.setUserData(nullptr);

        QRegularExpressionMatch matchHorizontalLine =
            regexHorizontalLine.match(block.text());
        if (matchHorizontalLine.hasMatch())
        {
            LineData *lineData = new LineData;
            block.setUserData(lineData);
        }
        parser.identifyFormatData(block);
    }

    this->selectRange.hideBlocks.erase(blockNumber);
    hideMKSymbolsFromPreviousSelectedBlocks(&this->selectRange);
    this->selectRange.showBlocks.insert(blockNumber);
    showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
}

void MkTextDocument::identifyUserData()
{
    checkMarkPositions.clear();
    linkPositions.clear();

    for (QTextBlock block = this->begin(); block != this->end();
         block = block.next())
    {
        block.setUserData(nullptr);
    }

    // One pass finds every multi-block construct (code fences and tables).
    const QVector<BlockStructure> structures =
        BlockStructureScanner(*this).scan();
    int structureIndex = 0;

    for (QTextBlock block = this->begin(); block != this->end();
         block = block.next())
    {
        const int blockNo = block.blockNumber();
        while (structureIndex < structures.size() &&
               structures.at(structureIndex).lastBlock < blockNo)
        {
            ++structureIndex;
        }

        const BlockStructure *structure =
            (structureIndex < structures.size() &&
             structures.at(structureIndex).contains(blockNo))
                ? &structures.at(structureIndex)
                : nullptr;

        if (structure && structure->kind == BlockStructure::Kind::CodeFence)
        {
            const BlockStructure::Role role = structure->roleAt(blockNo);
            BlockData::statusID status = BlockData::content;
            if (role == BlockStructure::Role::Start)
                status = BlockData::start;
            else if (role == BlockStructure::Role::End)
                status = BlockData::end;

            auto *data = new BlockData(status);
            data->setStartBlock(structure->firstBlock);
            data->setEndBlock(structure->lastBlock);
            block.setUserData(data);
            continue;
        }

        if (structure && structure->kind == BlockStructure::Kind::Table)
        {
            TableData::RowKind kind = TableData::Body;
            switch (structure->roleAt(blockNo))
            {
            case BlockStructure::Role::Header:
                kind = TableData::Header;
                break;
            case BlockStructure::Role::Delimiter:
                kind = TableData::Delimiter;
                break;
            default:
                kind = TableData::Body;
                break;
            }

            auto *table = new TableData(kind);
            table->setColumnCount(structure->columnCount);
            table->setAlignments(structure->alignments);
            table->setColumnWidths(structure->columnWidths);
            table->setTabStops(tableTabStops(structure->columnWidths));
            table->setTableRange(structure->firstBlock, structure->lastBlock);
            block.setUserData(table);

            const int textLength = block.text().size();
            if (kind == TableData::Delimiter)
            {
                // The delimiter row carries no visible text.
                table->setMaskSize(textLength);
                table->addMaskRange(0, textLength - 1);
                continue;
            }

            parser.identifyFormatData(block, table);
            continue;
        }

        if (regexHorizontalLine.match(block.text()).hasMatch())
        {
            block.setUserData(new LineData);
            continue;
        }

        parser.identifyFormatData(block);
    }
}

// NOTE(limit): column widths are measured in characters using the document
// font. A font or zoom change therefore needs a re-measure, which the editor
// triggers from wheelEvent()/setFont().
QVector<qreal> MkTextDocument::tableTabStops(const QVector<int> &columnWidths)
{
    // Convert the widest cell of every column into a pixel offset. The extra
    // padding keeps text from touching the drawn column separators.
    const QFontMetricsF metrics(this->defaultFont());
    const qreal spaceWidth =
        qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char(' ')));
    const qreal padding = 8.0;

    QVector<qreal> stops;
    stops.reserve(columnWidths.size() + 1);

    // The first stop is the left border; every following stop is a column
    // separator, and the cell text of column i starts at stops[i].
    qreal x = 0.0;
    stops.append(x);
    for (const int width : columnWidths)
    {
        const int characters = qMax(2, width) + 3;
        x += characters * spaceWidth + padding / 4.0;
        stops.append(x);
    }

    return stops;
}

void MkTextDocument::autoCompleteCodeBlock(int blockNumber, bool &success)
{
    QTextCursor editCursor(this->findBlockByNumber(blockNumber));
    QTextBlock currentBlock = this->findBlockByNumber(blockNumber);

    QRegularExpressionMatch matchCodeBlockSymbol =
        regexCodeBlock.match(currentBlock.text());
    if (matchCodeBlockSymbol.hasMatch())
    {
        editCursor.insertText(CODEBLOCK_SYMBOL);
        editCursor.insertBlock();
        success = true;
    }
    else
    {
        success = false;
    }
}

BlockData *MkTextDocument::checkValidCodeBlock(QTextBlock &block)
{
    if (!block.isValid())
        return nullptr;

    QTextBlockUserData *data = block.userData();
    BlockData *blockData = dynamic_cast<BlockData *>(data);
    if (blockData)
        return blockData;

    return nullptr;
}

void MkTextDocument::enterKeyPressedHandle(int blockNumber, int &newCursorPos,
                                           bool continueQuote)
{
    numberListDetect(blockNumber, newCursorPos, continueQuote);
}

void MkTextDocument::quoteLeftKeyPressedHandle(int blockNumber, bool &success)
{
    autoCompleteCodeBlock(blockNumber, success);
}

void MkTextDocument::checkRightClockOnCodeBlockHandle(int blockNumber,
                                                      bool &valid)
{
    QTextBlock codeBlockSuspect = this->findBlockByNumber(blockNumber);
    BlockData *blockData = checkValidCodeBlock(codeBlockSuspect);
    valid = (blockData != nullptr);
}

void MkTextDocument::selectBlockCopyHandle(int blockNumber, int &startPos,
                                           int &endPos)
{
    QTextBlock currentBlock = this->findBlockByNumber(blockNumber);

    // Tables copy like code blocks: the whole multi-block construct.
    if (auto *table = dynamic_cast<TableData *>(currentBlock.userData()))
    {
        const QTextBlock first =
            this->findBlockByNumber(table->getTableStartBlock());
        const QTextBlock last =
            this->findBlockByNumber(table->getTableEndBlock());
        if (!first.isValid() || !last.isValid())
            return;

        startPos = first.position();
        endPos = last.position() + last.length() - 1;
        return;
    }

    BlockData *blockData = checkValidCodeBlock(currentBlock);
    if (blockData)
    {
        if (blockData->getStatus() == BlockData::end ||
            blockData->getStatus() == BlockData::content)
        {
            while (blockData->getStatus() != BlockData::start)
            {
                currentBlock = currentBlock.previous();
                blockData = checkValidCodeBlock(currentBlock);
            }
        }
        if (blockData->getStatus() == BlockData::start)
        {
            startPos = currentBlock.next().position();
            while (blockData->getStatus() != BlockData::end)
            {
                currentBlock = currentBlock.next();
                blockData = checkValidCodeBlock(currentBlock);
                if (!blockData)
                {
                    endPos = startPos;
                    return;
                }
            }
            endPos = currentBlock.previous().position() +
                     currentBlock.previous().length() - 1;
        }
    }
}

void MkTextDocument::duplicateLineHandle(int blockNumber)
{
    QTextBlock currentBlock = this->findBlockByNumber(blockNumber);
    QTextCursor editCursor(currentBlock);
    QString lineText = currentBlock.text();
    editCursor.movePosition(QTextCursor::EndOfBlock);
    editCursor.insertBlock();
    editCursor.insertText(lineText);
}

void MkTextDocument::smartSelectionHandle(int blockNumber, QTextCursor &cursor)
{
    QTextBlock currentBlock = this->findBlockByNumber(blockNumber);
    QString lineText = currentBlock.text();
    int currentBlockPos = currentBlock.position();
    int start = cursor.selectionStart() - currentBlockPos;
    int end = cursor.selectionEnd() - currentBlockPos;

    int lastCharacter = lineText.length();
    QChar firstCharacterCheck;
    QChar endCharacterCheck;

    if (!cursor.hasSelection())
    {
        cursor.movePosition(QTextCursor::StartOfWord);
        cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        return;
    }

    if ((start > 0) && (end < lastCharacter))
    {
        firstCharacterCheck = lineText.at(start - 1);
        endCharacterCheck = lineText.at(end);
        if (((firstCharacterCheck == '\"') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == '{') && (endCharacterCheck == '}')) ||
            ((firstCharacterCheck == '(') && (endCharacterCheck == ')')) ||
            ((firstCharacterCheck == '[') && (endCharacterCheck == ']')))
        {
            if (start > 0)
                start--;
            if (end < lastCharacter)
                end++;

            cursor.setPosition(start + currentBlockPos);
            cursor.setPosition(end + currentBlockPos, QTextCursor::KeepAnchor);
            return;
        }

        if (((firstCharacterCheck == '{') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == '{') && (endCharacterCheck == ')')) ||
            ((firstCharacterCheck == '{') && (endCharacterCheck == ']')) ||
            ((firstCharacterCheck == '(') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == '(') && (endCharacterCheck == '}')) ||
            ((firstCharacterCheck == '(') && (endCharacterCheck == ']')) ||
            ((firstCharacterCheck == '[') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == '[') && (endCharacterCheck == ')')) ||
            ((firstCharacterCheck == '[') && (endCharacterCheck == ']')))
        {
            if (end < lastCharacter)
                end++;

            cursor.setPosition(start + currentBlockPos);
            cursor.setPosition(end + currentBlockPos, QTextCursor::KeepAnchor);
            return;
        }

        if (((firstCharacterCheck == '\"') && (endCharacterCheck == '}')) ||
            ((firstCharacterCheck == '\"') && (endCharacterCheck == ')')) ||
            ((firstCharacterCheck == '\"') && (endCharacterCheck == ']')) ||
            ((firstCharacterCheck == '}') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == '}') && (endCharacterCheck == ')')) ||
            ((firstCharacterCheck == '}') && (endCharacterCheck == ']')) ||
            ((firstCharacterCheck == ')') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == ')') && (endCharacterCheck == '}')) ||
            ((firstCharacterCheck == ')') && (endCharacterCheck == ']')) ||
            ((firstCharacterCheck == ']') && (endCharacterCheck == '\"')) ||
            ((firstCharacterCheck == ']') && (endCharacterCheck == ')')) ||
            ((firstCharacterCheck == ']') && (endCharacterCheck == ']')))
        {
            if (start > 0)
                start--;

            cursor.setPosition(start + currentBlockPos);
            cursor.setPosition(end + currentBlockPos, QTextCursor::KeepAnchor);
            return;
        }
    }

    while (start > 0)
    {
        start--;
        if (!lineText.at(start).isSpace())
            break;
    }

    while (start >= 0)
    {
        if (lineText.at(start) == QChar('\"'))
        {
            if (!(cursor.selectedText().contains("\"")))
            {
                firstCharacterCheck = '\"';
                break;
            }
        }
        else if (lineText.at(start) == QChar('{'))
        {
            if (!(cursor.selectedText().contains("{") ||
                  cursor.selectedText().contains("}")))
            {
                firstCharacterCheck = '{';
                break;
            }
        }
        else if (lineText.at(start) == QChar('('))
        {
            if (!(cursor.selectedText().contains("(") ||
                  cursor.selectedText().contains(")")))
            {
                firstCharacterCheck = '(';
                break;
            }
        }
        else if (lineText.at(start) == QChar('['))
        {
            if (!(cursor.selectedText().contains("[") ||
                  cursor.selectedText().contains("]")))
            {
                firstCharacterCheck = '[';
                break;
            }
        }
        else if (lineText.at(start).isSpace())
        {
            firstCharacterCheck = ' ';
            break;
        }
        start--;
    }
    start++;

    if (end < lastCharacter)
    {
        if (lineText.at(end).isSpace())
        {
            while (end < (lastCharacter - 1))
            {
                end++;
                if (!lineText.at(end).isSpace())
                    break;
            }
        }
    }

    while (end < lastCharacter)
    {
        if (lineText.at(end) == QChar('\"'))
        {
            if (!(cursor.selectedText().contains("\"")))
            {
                endCharacterCheck = '\"';
                break;
            }
        }
        else if (lineText.at(end) == QChar('}'))
        {
            if (!(cursor.selectedText().contains("{") ||
                  cursor.selectedText().contains("}")))
            {
                endCharacterCheck = '}';
                break;
            }
        }
        else if (lineText.at(end) == QChar(')'))
        {
            if (!(cursor.selectedText().contains("(") ||
                  cursor.selectedText().contains(")")))
            {
                endCharacterCheck = ')';
                break;
            }
        }
        else if (lineText.at(end) == QChar(']'))
        {
            if (!(cursor.selectedText().contains("[") ||
                  cursor.selectedText().contains("]")))
            {
                endCharacterCheck = ']';
                break;
            }
        }
        else if (lineText.at(end).isSpace())
        {
            endCharacterCheck = ' ';
            break;
        }
        end++;
    }

    cursor.setPosition(start + currentBlockPos);
    cursor.setPosition(end + currentBlockPos, QTextCursor::KeepAnchor);
}

QString MkTextDocument::sourceText() const
{
    // Block counts can legitimately diverge only while a structural edit is in
    // flight; in that state the rendered document already holds plain source.
    if (this->blockCount() != rawDocument.blockCount())
        return this->toPlainText();

    QStringList lines;
    lines.reserve(this->blockCount());

    for (QTextBlock block = this->begin(); block.isValid(); block = block.next())
    {
        const QTextBlock rawBlock =
            rawDocument.findBlockByNumber(block.blockNumber());
        if (isBlockRendered(block) && rawBlock.isValid())
            lines.append(rawBlock.text());
        else
            lines.append(block.text());
    }

    return lines.join(QLatin1Char('\n'));
}

void MkTextDocument::saveRawDocumentHandler()
{
    // Never copy the rendered text wholesale: it may contain tabs instead of
    // table pipes or object characters instead of image Markdown.
    this->rawDocument.setPlainText(sourceText());
}

void MkTextDocument::saveSingleRawBlockHandler(int blockNumber)
{
    const QTextBlock rawBlock = this->rawDocument.findBlockByNumber(blockNumber);
    const QTextBlock viewBlock = this->findBlockByNumber(blockNumber);
    if (!rawBlock.isValid() || !viewBlock.isValid())
        return;

    const QString sourceMarkdown = rawBlock.text();
    const QString updatedMarkdown =
        restoreImageMarkdown(viewBlock, sourceMarkdown);

    QTextCursor rawCursor(&this->rawDocument);
    rawCursor.setPosition(rawBlock.position());
    rawCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    rawCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    rawCursor.insertText(updatedMarkdown);
}

void MkTextDocument::saveEnterPressRawBlockHandler(int blockNumber,
                                                   QString text)
{
    QTextCursor rawCursor(&this->rawDocument);
    rawCursor.setPosition(
        this->rawDocument.findBlockByNumber(blockNumber).position());
    rawCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    rawCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    rawCursor.insertText(text);
}

void MkTextDocument::hideMKSymbolsFromPreviousSelectedBlocks(
    SelectRange *const range)
{
    emit disconnectCursorPos();
    int fontSize = this->defaultFont().pointSize();
    FormatCollection formatCollection(fontSize, this->palette);

    if (disableMarkdownState)
    {
        range->hideBlocks.clear();
        return;
    }

    QSet<int> hiddenBlocks;
    QTextBlock block;
    foreach (int num, range->hideBlocks)
    {
        if (range->showBlocks.find(num) == range->showBlocks.end())
        {
            hiddenBlocks.insert(num);
            block = this->findBlockByNumber(num);
            if (!block.isValid())
                continue;

            QTextBlockUserData *data = block.userData();
            if (data == nullptr)
            {
                MarkdownRenderer::resetTextBlockFormat(block);
                continue;
            }

            BlockData *blockData = dynamic_cast<BlockData *>(data);
            if (blockData)
            {
                MarkdownRenderer::resetTextBlockFormat(block);
                MarkdownRenderer::showHideCodeBlock(this, blockData, true, fontSize);
                if (blockData->getStatus() == BlockData::content)
                    MarkdownRenderer::setCodeBlockMargin(block, fontSize * 10 / 3, fontSize, 0);
                continue;
            }

            LineData *lineData = dynamic_cast<LineData *>(data);
            if (lineData && !lineData->isHidden())
            {
                MarkdownRenderer::resetTextBlockFormat(block);
                lineData->setHidden(true);
                lineData->setDraw(true);
                MarkdownRenderer::hideSymbols(block, lineData->getSymbol());
                continue;
            }

            FormatData *formatData = dynamic_cast<FormatData *>(data);
            if (formatData && !formatData->isHidden())
            {
                formatData->setHidden(true);

                const QBitArray &mask = formatData->getMask();
                const int sourceLength = mask.size();
                if (sourceLength == 0)
                    continue;

                // Inline images replace a multi-character expression with one
                // object character. The block may therefore hold either the
                // untouched source line or a line where those expressions are
                // already collapsed; accept both and reject anything else.
                const int imageCollapse = formatData->totalCollapsedChars();
                const int textLength = block.text().size();
                const bool imagesCollapsed =
                    imageCollapse > 0 &&
                    textLength == sourceLength - imageCollapse;
                const bool sourceFrame = textLength == sourceLength;

                if (!imagesCollapsed && !sourceFrame)
                    continue;

                if (!formatData->isFormatted())
                {
                    for (QVector<FragmentData *>::Iterator it =
                             formatData->formats_begin();
                         it < formatData->formats_end(); it++)
                    {
                        // Images are inserted after masking so that they do
                        // not invalidate the remaining source offsets.
                        if ((*it)->getStatus() == FragmentData::IMAGE)
                            continue;

                        MarkdownRenderer::applyMkFormat(this, block, *(*it),
                                                       (*it)->getStatus(),
                                                       formatCollection);
                    }
                }

                // Map a source offset to the current text offset. Going from
                // the end towards the start keeps every earlier offset valid
                // while characters are deleted.
                const QVector<QPair<int, int>> imageSpans =
                    formatData->imageSpans();
                auto textPositionFor = [&](int sourcePos)
                {
                    int shift = 0;
                    if (imagesCollapsed)
                    {
                        for (const auto &span : imageSpans)
                        {
                            if (span.second < sourcePos)
                                shift += qMax(0, span.second - span.first);
                        }
                    }
                    return sourcePos - shift;
                };

                QTextCursor cursor(block);
                for (int index = sourceLength - 1; index >= 0; --index)
                {
                    if (!mask.testBit(index))
                        continue;

                    const int textPos = textPositionFor(index);
                    if (textPos < 0 || textPos >= block.length() - 1)
                        continue;

                    cursor.setPosition(block.position() + textPos);
                    cursor.movePosition(QTextCursor::NextCharacter,
                                        QTextCursor::KeepAnchor);

                    // Table rows replace '|' with a tab. The replacement is a
                    // single character, so every later offset stays valid.
                    if (formatData->hasCharacterReplacement(index))
                        cursor.insertText(QString(
                            formatData->characterReplacementAt(index)));
                    else
                        cursor.removeSelectedText();
                }

                if (!imagesCollapsed)
                    MarkdownRenderer::insertImages(this, block, formatData);

                if (auto *table = dynamic_cast<TableData *>(formatData))
                {
                    MarkdownRenderer::applyTableRowFormat(
                        this, block, table, formatCollection);
                }

                MarkdownRenderer::applyCheckBoxLinkEffect(
                    formatData, block, checkMarkPositions, linkPositions);
            }
        }
    }

    foreach (int num, hiddenBlocks) range->hideBlocks.erase(num);

    emit connectCursorPos();
}

void MkTextDocument::showMKSymbolsFromCurrentSelectedBlocks(
    SelectRange *const range)
{
    emit disconnectCursorPos();
    int fontSize = this->defaultFont().pointSize();
    FormatCollection formatCollection(fontSize, this->palette);
    QTextBlock block;

    foreach (int num, range->showBlocks)
    {
        if (range->hideBlocks.find(num) == range->hideBlocks.end())
        {
            range->hideBlocks.insert(num);
            block = this->findBlockByNumber(num);
            if (!block.isValid())
                continue;

            QTextBlockUserData *data = block.userData();
            if (data == nullptr)
            {
                MarkdownRenderer::resetTextBlockFormat(block);
                continue;
            }

            BlockData *blockData = dynamic_cast<BlockData *>(data);
            if (blockData)
            {
                MarkdownRenderer::resetTextBlockFormat(block);
                MarkdownRenderer::showHideCodeBlock(this, blockData, false, fontSize);
                if (blockData->getStatus() == BlockData::content)
                    MarkdownRenderer::setCodeBlockMargin(block, fontSize * 10 / 3, fontSize, 0);
                continue;
            }

            LineData *lineData = dynamic_cast<LineData *>(data);
            if (lineData && lineData->isHidden())
            {
                lineData->setHidden(false);
                lineData->setDraw(false);
                MarkdownRenderer::showSymbols(block, lineData->getSymbol());
                continue;
            }

            FormatData *formatData = dynamic_cast<FormatData *>(data);
            if (formatData)
            {
                bool isPosInBlockAtMax =
                    (range->currentposInBlock == (block.length() - 1));
                bool isHidden = formatData->isHidden();
                MarkdownRenderer::resetTextBlockFormat(block);
                MarkdownRenderer::removeCheckBoxLinkMousePosition(block, checkMarkPositions, linkPositions);

                // Re-parsing replaces the user data, so a table row clones its
                // table layout first; otherwise the row would forget its
                // columns, tab stops, and pipe-to-tab replacement.
                TableData *freshTable = nullptr;
                if (auto *table = dynamic_cast<TableData *>(formatData))
                    freshTable = table->cloneLayout();

                QTextCursor cursor(block);
                cursor.movePosition(QTextCursor::StartOfBlock);
                cursor.movePosition(QTextCursor::EndOfBlock,
                                    QTextCursor::KeepAnchor);
                QString line =
                    rawDocument.findBlockByNumber(block.blockNumber()).text();
                cursor.insertText(line);

                if (freshTable)
                {
                    freshTable->setMaskSize(line.size());
                    block.setUserData(freshTable);
                    parser.identifyFormatData(block, freshTable);
                }
                else
                {
                    parser.identifyFormatData(block);
                }

                FormatData *newData =
                    dynamic_cast<FormatData *>(block.userData());
                if (newData)
                {
                    newData->setFormatted(true);
                    // Apply text-only formatting first. Image rendering replaces
                    // Markdown source with an inline object and changes later offsets.
                    for (QVector<FragmentData *>::Iterator it =
                             newData->formats_begin();
                         it < newData->formats_end(); it++)
                    {
                        if ((*it)->getStatus() != FragmentData::IMAGE)
                            MarkdownRenderer::applyMkFormat(this, block, *(*it), (*it)->getStatus(),
                                          formatCollection);
                    }

                    // Render images right-to-left so replacing one image cannot
                    // invalidate the source offsets of images earlier in the line.
                    for (auto it = newData->formats_rbegin();
                         it != newData->formats_rend(); ++it)
                    {
                        if ((*it)->getStatus() == FragmentData::IMAGE)
                            MarkdownRenderer::applyMkFormat(this, block, *(*it), (*it)->getStatus(),
                                          formatCollection);
                    }

                    // Only move the caret when this block is shown because
                    // the caret or selection is inside it. A block revealed by
                    // hovering must not steal the caret position.
                    if (isHidden &&
                        cursorVisibleBlocks.contains(block.blockNumber()))
                    {
                        range->currentBlockNo = block.blockNumber();
                        range->currentposInBlock =
                            isPosInBlockAtMax ? block.length() - 1
                                              : newData->getCalculatedCursorPos(
                                                    range->currentposInBlock);
                    }
                }
            }
        }
    }
    range->showBlocks.clear();

    if (disableMarkdownState)
    {
        range->hideBlocks.clear();
        return;
    }

    emit connectCursorPos();
}

void MkTextDocument::pushCheckBoxHandle(const int position)
{
    QTextBlock block = findBlock(position);
    int blockNo = block.blockNumber();
    QTextBlockUserData *data = block.userData();
    FormatData *formatData = dynamic_cast<FormatData *>(data);

    if (!formatData)
        return;

    // Map the clicked rendered offset back to the source line. This accounts
    // for hidden markers and for inline images that collapsed to one character.
    const int renderedPosInBlock = position - block.position();
    const int rawPosInBlock =
        formatData->getCalculatedCursorPos(renderedPosInBlock);

    const int startOfCheckBoxInBlock = qMax(0, rawPosInBlock - 5);
    QString rawText = this->rawDocument.findBlockByNumber(blockNo).text();
    QTextCursor cursor(this);
    cursor.setPosition(position);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

    for (QVector<FragmentData *>::Iterator it =
             formatData->hiddenFormats_begin();
         it < formatData->hiddenFormats_end(); it++)
    {
        if ((*it)->getStatus() == FragmentData::CHECKED_END)
        {
            if ((*it)->getStart() == renderedPosInBlock)
            {
                int replaceStartPos =
                    rawText.indexOf("- [x]", startOfCheckBoxInBlock);
                if (replaceStartPos != -1)
                    rawText.replace(replaceStartPos, 5, "- [ ]");
                (*it)->setStatus(FragmentData::UNCHECKED_END);
                cursor.insertText(UNCHECKED_PIC);
                break;
            }
        }
        else if ((*it)->getStatus() == FragmentData::UNCHECKED_END)
        {
            if ((*it)->getStart() == renderedPosInBlock)
            {
                int replaceStartPos =
                    rawText.indexOf("- [ ]", startOfCheckBoxInBlock);
                if (replaceStartPos != -1)
                    rawText.replace(replaceStartPos, 5, "- [x]");
                (*it)->setStatus(FragmentData::CHECKED_END);
                cursor.insertText(CHECKED_PIC);
                break;
            }
        }
    }

    QTextCursor cursorRaw(&rawDocument);
    cursorRaw.setPosition(
        rawDocument.findBlockByLineNumber(blockNo).position());
    cursorRaw.movePosition(QTextCursor::StartOfBlock);
    cursorRaw.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursorRaw.insertText(rawText);
}

void MkTextDocument::pushLinkHandle(const int blockNo, const int posInBlock)
{
    QTextBlock block = findBlockByNumber(blockNo);
    QTextBlockUserData *data = block.userData();
    FormatData *formatData = dynamic_cast<FormatData *>(data);

    if (!formatData)
        return;

    for (QVector<FragmentData *>::Iterator it =
             formatData->hiddenFormats_begin();
         it < formatData->hiddenFormats_end(); it++)
    {
        if ((*it)->getStatus() == FragmentData::LINK_TITLE &&
            (*it)->getStart() == posInBlock)
        {
            const QString *link =
                formatData->getHiddenLinkUrl((*it)->getStart());
            if (link)
                QDesktopServices::openUrl(QUrl(*link));
        }
    }
}

void MkTextDocument::autoInsertSymbolHandle(const int position,
                                            int &newPosition)
{
    QTextCursor cursor(this);
    cursor.setPosition(position);
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);

    QString text = cursor.selectedText().toLower();
    QString checkedSymbol =
        QString(CHECK_SYMBOL_START) + QString(CHECKED_SYMBOL_END) + " ";
    QString unCheckSymbol =
        QString(CHECK_SYMBOL_START) + QString(UNCHECKED_SYMBOL_END) + " ";

    if (text == "check" || text == "ch")
    {
        cursor.insertText(checkedSymbol);
    }
    else if (text == "uncheck" || text == "uch")
    {
        cursor.insertText(unCheckSymbol);
    }
    else if (text == "link" || text == "lk")
    {
        cursor.insertText(QString(LINK_SYMBOL_TITLE_START) +
                          QString(LINK_SYMBOL_MID) +
                          QString(LINK_SYMBOL_URL_END));
        cursor.setPosition(cursor.position() - 2);
    }
    else
    {
        cursor.setPosition(position);
        cursor.insertText("    ");
    }

    newPosition = cursor.positionInBlock();
}

void MkTextDocument::setMarkdownHandle(bool state)
{
    disableMarkdownState = !state;

    if (disableMarkdownState)
    {
        this->setPlainText(this->rawDocument.toPlainText());
        for (int num = 0; num < this->blockCount(); num++)
            this->selectRange.showBlocks.insert(num);

        showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
    }
    else
    {
        for (int num = 0; num < this->blockCount(); num++)
            this->selectRange.hideBlocks.insert(num);

        hideMKSymbolsFromPreviousSelectedBlocks(&this->selectRange);
        this->selectRange.hideBlocks.clear();
    }
}

void MkTextDocument::cursorUpdateHandle(const int blockNo,
                                        const int characterNo)
{
    this->blockNo = blockNo;
    this->characterNo = characterNo;
}

void MkTextDocument::undoStackPush(const EditSnapshot &snapshot)
{
    undoStack.push(new EditCommand(this, snapshot));
}

void MkTextDocument::undoStackUndo(EditRestore &restore)
{
    restore = EditRestore{};
    if (!undoStack.canUndo())
        return;

    undoStack.undo();
    // undo() decrements the current index, so the undone command sits at
    // index() afterwards.
    if (const auto *command =
            dynamic_cast<const EditCommand *>(
                undoStack.command(undoStack.index())))
        restore = command->restore();
    restore.success = true;
}

void MkTextDocument::undoStackRedo(EditRestore &restore)
{
    restore = EditRestore{};
    if (!undoStack.canRedo())
        return;

    undoStack.redo();
    // redo() increments the current index, so the redone command sits at
    // index() - 1.
    if (const auto *command =
            dynamic_cast<const EditCommand *>(
                undoStack.command(undoStack.index() - 1)))
        restore = command->restore();
    restore.success = true;
}

void MkTextDocument::numberListDetect(int blockNumber, int &newCursorPosition,
                                      bool continueQuote)
{
    QTextCursor cursor(this->findBlockByNumber(blockNumber));
    QTextBlock currentBlock = this->findBlockByNumber(blockNumber - 1);
    QString lineText = currentBlock.text();

    // Table continuation: pressing Enter inside a table starts a new row with
    // the same number of columns. Ctrl+Enter skips this and leaves the table.
    if (continueQuote)
    {
        if (auto *table =
                dynamic_cast<TableData *>(currentBlock.userData()))
        {
            const int columns = qMax(1, table->getColumnCount());
            QString row = QStringLiteral("|");
            for (int column = 0; column < columns; ++column)
                row += QStringLiteral("   |");

            cursor.insertText(row);
            // Put the caret in the first cell, right after "| ".
            cursor.setPosition(cursor.block().position() + 2);
            newCursorPosition = 2;
            return;
        }
    }

    // Blockquote continuation: reuse the exact leading ">" marker(s) so
    // pressing Enter keeps the quote (and its nesting) going.
    if (continueQuote)
    {
        const QString quotePrefix = MarkdownParser::blockQuotePrefix(lineText);
        if (!quotePrefix.isEmpty())
        {
            cursor.insertText(quotePrefix);
            newCursorPosition = cursor.positionInBlock();
            return;
        }
    }

    regexNumbering.setPattern("^( {0,}|\\s{2,})\\d+\\.\\s*.*$");
    QRegularExpressionMatch matchNumbering = regexNumbering.match(lineText);

    regexBulletPoints.setPattern("^\\s*-\\s*");
    QRegularExpressionMatch matchBulletPoint =
        regexBulletPoints.match(lineText);

    QRegularExpressionMatch matchCheckBox = regexCheckBox.match(lineText);
    QRegularExpressionMatch matchBulletCheckBox =
        regexBulletCheckBox.match(lineText);

    if (matchNumbering.hasMatch())
    {
        int spaces = numberListGetSpaces(matchNumbering.captured(0));
        cursor.insertText(QString("").leftJustified(spaces, ' '));
        cursor.insertText(numberListGetNextNumber(matchNumbering.captured(0)));
    }
    else if (matchBulletCheckBox.hasMatch())
    {
        int spaces = numberListGetSpaces(matchBulletPoint.captured(0));
        cursor.insertText(QString("").leftJustified(spaces, ' '));
        if (matchCheckBox.captured(0).contains("x"))
            cursor.insertText("- - [x]  ");
        else
            cursor.insertText("- - [ ]  ");
    }
    else if (matchCheckBox.hasMatch())
    {
        int spaces = numberListGetSpaces(matchBulletPoint.captured(0));
        cursor.insertText(QString("").leftJustified(spaces, ' '));
        if (matchCheckBox.captured(0).contains("x"))
            cursor.insertText("- [x]  ");
        else
            cursor.insertText("- [ ]  ");
    }
    else if (matchBulletPoint.hasMatch())
    {
        int spaces = numberListGetSpaces(matchBulletPoint.captured(0));
        cursor.insertText(QString("").leftJustified(spaces, ' '));
        cursor.insertText("- ");
    }

    newCursorPosition = cursor.positionInBlock();
}

int MkTextDocument::numberListGetSpaces(const QString &text)
{
    regexNumbering.setPattern("^\\s+");
    QRegularExpressionMatch matchSpace = regexNumbering.match(text);
    if (matchSpace.hasMatch())
        return matchSpace.captured(0).length();
    return 0;
}

QString MkTextDocument::numberListGetNextNumber(const QString &text)
{
    regexNumbering.setPattern("[0-9]+");
    QRegularExpressionMatch matchSpace = regexNumbering.match(text);
    if (matchSpace.hasMatch())
    {
        int number = matchSpace.captured(0).toInt();
        return QString::number(number + 1) + ". ";
    }
    return "";
}

void MkTextDocument::revealAllMkSymbols()
{
    if (disableMarkdownState)
        return;

    for (int num = 0; num < this->blockCount(); ++num)
    {
        this->selectRange.hideBlocks.erase(num);
        this->selectRange.showBlocks.insert(num);
    }

    cursorVisibleBlocks.clear();
    for (int num = 0; num < this->blockCount(); ++num)
        cursorVisibleBlocks.insert(num);

    showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
}
