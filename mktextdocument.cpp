#include "mktextdocument.h"

#include <QChar>
#include <QColor>
#include <QTextBlockFormat>
#include <QTextImageFormat>

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------
namespace {

// CommonMark defines "Unicode punctuation" as any character in the Unicode
// P (punctuation) or S (symbol) categories.  Qt exposes both directly.
bool isUnicodePunctuation(QChar c)
{
    if (c.isNull())
        return false;
    return c.isPunct() || c.isSymbol();
}

// A delimiter run is "protected" if it sits entirely inside one of the
// ranges returned by parseInlineCodeSpans().  Because code spans are
// delimited by backticks, a delimiter run is either fully inside a range
// or fully outside - there is no partial overlap.
bool isProtectedPosition(int pos, const QVector<QPair<int, int>> &ranges)
{
    for (const auto &r : ranges)
    {
        if (pos >= r.first && pos <= r.second)
            return true;
    }
    return false;
}

// A small inline-image scanner shared by rendering and raw-Markdown sync.
// Unlike the old regular expression, it accepts angle-bracket destinations
// containing spaces and balanced parentheses in ordinary destinations.
struct MarkdownImageExpression
{
    int start = -1;
    int end = -1;  // exclusive
    QString source;
    QString url;
    QString alt;
};

bool isMarkdownEscaped(const QString &text, int position)
{
    int backslashes = 0;
    for (int i = position - 1; i >= 0 && text.at(i) == QLatin1Char('\\'); --i)
        ++backslashes;
    return (backslashes % 2) != 0;
}

QVector<MarkdownImageExpression> parseMarkdownImages(const QString &text)
{
    QVector<MarkdownImageExpression> images;
    int i = 0;
    while (i + 3 < text.size())
    {
        if (text.at(i) != QLatin1Char('!') ||
            text.at(i + 1) != QLatin1Char('[') ||
            isMarkdownEscaped(text, i))
        {
            ++i;
            continue;
        }

        const int altStart = i + 2;
        int cursor = altStart;
        int bracketDepth = 0;
        int altEnd = -1;
        for (; cursor < text.size(); ++cursor)
        {
            if (isMarkdownEscaped(text, cursor))
                continue;
            const QChar c = text.at(cursor);
            if (c == QLatin1Char('['))
                ++bracketDepth;
            else if (c == QLatin1Char(']'))
            {
                if (bracketDepth == 0)
                {
                    altEnd = cursor;
                    break;
                }
                --bracketDepth;
            }
        }

        if (altEnd < 0 || altEnd + 1 >= text.size() ||
            text.at(altEnd + 1) != QLatin1Char('('))
        {
            ++i;
            continue;
        }

        const int destinationStart = altEnd + 2;
        int urlStart = destinationStart;
        int urlEnd = -1;
        int expressionEnd = -1;

        if (destinationStart < text.size() &&
            text.at(destinationStart) == QLatin1Char('<'))
        {
            urlStart = destinationStart + 1;
            int angleEnd = urlStart;
            for (; angleEnd < text.size(); ++angleEnd)
            {
                if (text.at(angleEnd) == QLatin1Char('>') &&
                    !isMarkdownEscaped(text, angleEnd))
                    break;
            }
            if (angleEnd >= text.size())
            {
                ++i;
                continue;
            }
            urlEnd = angleEnd;
            int close = angleEnd + 1;
            bool inTitle = false;
            QChar titleQuote;
            for (; close < text.size(); ++close)
            {
                const QChar c = text.at(close);
                if (isMarkdownEscaped(text, close))
                    continue;
                if (inTitle)
                {
                    if (c == titleQuote)
                        inTitle = false;
                }
                else if (c == QLatin1Char('"') || c == QLatin1Char('\''))
                {
                    inTitle = true;
                    titleQuote = c;
                }
                else if (c == QLatin1Char(')'))
                    break;
            }
            if (close >= text.size())
            {
                ++i;
                continue;
            }
            expressionEnd = close + 1;
        }
        else
        {
            int depth = 0;
            cursor = destinationStart;
            for (; cursor < text.size(); ++cursor)
            {
                if (isMarkdownEscaped(text, cursor))
                    continue;
                const QChar c = text.at(cursor);
                if (c.isSpace() && depth == 0)
                    break;
                if (c == QLatin1Char('('))
                    ++depth;
                else if (c == QLatin1Char(')'))
                {
                    if (depth == 0)
                        break;
                    --depth;
                }
            }

            urlEnd = cursor;
            if (cursor < text.size() && text.at(cursor) == QLatin1Char(')'))
            {
                expressionEnd = cursor + 1;
            }
            else
            {
                // A title may follow a destination separated by whitespace.
                bool inTitle = false;
                QChar titleQuote;
                int close = cursor;
                for (; close < text.size(); ++close)
                {
                    const QChar c = text.at(close);
                    if (isMarkdownEscaped(text, close))
                        continue;
                    if (inTitle)
                    {
                        if (c == titleQuote)
                            inTitle = false;
                    }
                    else if (c == QLatin1Char('"') ||
                             c == QLatin1Char('\''))
                    {
                        inTitle = true;
                        titleQuote = c;
                    }
                    else if (c == QLatin1Char(')'))
                        break;
                }
                if (close >= text.size())
                {
                    ++i;
                    continue;
                }
                expressionEnd = close + 1;
            }
        }

        if (urlEnd < urlStart || expressionEnd <= i)
        {
            ++i;
            continue;
        }

        MarkdownImageExpression image;
        image.start = i;
        image.end = expressionEnd;
        image.source = text.mid(i, expressionEnd - i);
        image.url = text.mid(urlStart, urlEnd - urlStart);
        image.alt = text.mid(altStart, altEnd - altStart);
        images.append(image);
        i = expressionEnd;
    }
    return images;
}

// QTextBlock::text() exposes an inline image as U+FFFC, whereas the raw
// document stores its Markdown syntax. Restore each original expression in
// source order so valid destinations with spaces or balanced parentheses
// survive an edit to the rendered block.
QString restoreImageMarkdown(const QTextBlock &renderedBlock,
                             const QString &sourceMarkdown)
{
    const QString renderedText = renderedBlock.text();
    const auto images = parseMarkdownImages(sourceMarkdown);
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

void MkTextDocument::cursorPosChangedHandle(SelectRange *const range,
                                            const bool readOnly)
{
    if (range && !readOnly)
    {
        int start;
        int end;

        this->selectRange.hasSelection = range->hasSelection;
        if (range->hasSelection)
        {
            start = std::min(range->selectionFirstStartBlock,
                             range->selectionEndBlock);
            end = std::max(range->selectionFirstStartBlock,
                           range->selectionEndBlock);
        }
        else
        {
            this->selectRange.currentposInBlock = range->currentposInBlock;
            this->selectRange.currentBlockNo = range->currentBlockNo;
            start = end = range->currentBlockNo;
        }

        for (int num = start; num <= end; num++)
        {
            this->selectRange.hideBlocks.erase(num);
            this->selectRange.showBlocks.insert(num);
        }

        hideMKSymbolsFromPreviousSelectedBlocks(&this->selectRange);
        showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);

        range->currentBlockNo = this->selectRange.currentBlockNo;
        range->currentposInBlock = this->selectRange.currentposInBlock;
    }
}

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

    this->selectRange.showBlocks.insert(blockNumber);
    showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
}

void MkTextDocument::applyMkSingleBlockHandle(int blockNumber)
{
    QTextBlock block = this->findBlockByNumber(blockNumber);
    QTextBlockUserData *data = block.userData();

    BlockData *blockData = dynamic_cast<BlockData *>(data);
    if (!blockData)
    {
        block.setUserData(nullptr);

        QRegularExpressionMatch matchHorizontalLine =
            regexHorizontalLine.match(block.text());
        if (matchHorizontalLine.hasMatch())
        {
            LineData *lineData = new LineData;
            block.setUserData(lineData);
        }
        identifyFormatData(block);

        this->selectRange.hideBlocks.erase(blockNumber);
        hideMKSymbolsFromPreviousSelectedBlocks(&this->selectRange);
        this->selectRange.showBlocks.insert(blockNumber);
        showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
    }
}

void MkTextDocument::identifyUserData()
{
    checkMarkPositions.clear();
    linkPositions.clear();

    QList<BlockData *> codeBlockBody;
    int startBlock;
    BlockData *code;

    bool openBlock = false;
    for (QTextBlock block = this->begin(); block != this->end();
         block = block.next())
    {
        block.setUserData(nullptr);

        QRegularExpressionMatch matchCodeBlock =
            regexCodeBlock.match(block.text());
        if (matchCodeBlock.hasMatch())
        {
            if (!openBlock)
            {
                openBlock = true;
                code = new BlockData(BlockData::start);
                startBlock = block.blockNumber();
                block.setUserData(code);
                codeBlockBody.append(code);
            }
            else
            {
                openBlock = false;
                code = new BlockData(BlockData::end);
                block.setUserData(code);
                codeBlockBody.append(code);

                foreach (auto codeBlock, codeBlockBody)
                {
                    codeBlock->setStartBlock(startBlock);
                    codeBlock->setEndBlock(block.blockNumber());
                }
                codeBlockBody.clear();
            }
            continue;
        }

        if (openBlock)
        {
            code = new BlockData(BlockData::content);
            block.setUserData(code);
            codeBlockBody.append(code);
            continue;
        }

        QRegularExpressionMatch matchHorizontalLine =
            regexHorizontalLine.match(block.text());
        if (matchHorizontalLine.hasMatch())
        {
            block.setUserData(new LineData);
            continue;
        }

        identifyFormatData(block);
    }
}

void MkTextDocument::identifyUserData(QTextBlock &block)
{
    QTextBlockUserData *data = block.userData();
    BlockData *blockData = dynamic_cast<BlockData *>(data);
    if (blockData)
        return;

    block.setUserData(nullptr);

    QRegularExpressionMatch matchHorizontalLine =
        regexHorizontalLine.match(block.text());
    if (matchHorizontalLine.hasMatch())
    {
        LineData *lineData = new LineData;
        block.setUserData(lineData);
    }
    else
    {
        identifyFormatData(block);
    }
}

void MkTextDocument::formatAllLines(const QTextDocument &original,
                                    MkTextDocument &formatted)
{
    bool openBlock = false;

    QTextCursor cursor(&formatted);
    cursor.movePosition(QTextCursor::Start);

    for (QTextBlock block = original.begin(); block != original.end();
         block = block.next())
    {
        cursor.insertBlock();
        QString blockText = block.text();

        QRegularExpressionMatch matchCodeBlock =
            regexCodeBlock.match(blockText);
        if (matchCodeBlock.hasMatch())
        {
            BlockData *blockData = new BlockData;
            cursor.block().setUserData(blockData);

            if (!openBlock)
            {
                openBlock = true;
                blockData->setStatus(BlockData::start);
            }
            else
            {
                openBlock = false;
                blockData->setStatus(BlockData::end);
            }
        }
        else
        {
            if (openBlock)
            {
                BlockData *blockData = new BlockData;
                blockData->setStatus(BlockData::content);
                cursor.block().setUserData(blockData);
            }
            else
            {
                QRegularExpressionMatch matchHorizontalLine =
                    regexHorizontalLine.match(block.text());
                if (matchHorizontalLine.hasMatch())
                {
                    LineData *lineData = new LineData;
                    cursor.block().setUserData(lineData);
                }
                QTextBlock inserted = cursor.block();
                identifyFormatData(inserted);
            }
        }
        cursor.insertText(blockText);
    }
}

void MkTextDocument::resetAllLoc()
{
    locItalicA.reset();
    locItalicU.reset();
    locBoldA.reset();
    locBoldU.reset();
    locCheck.reset();
}

void MkTextDocument::identifyFormatData(QTextBlock &block)
{
    resetFormatLocation();
    FormatData *formatData = new FormatData;
    QString text(block.text());
    QString textLink = text;
    QString test;

    formatData->setMaskSize(block.text().size());

    int index1 = 0;
    // CommonMark blockquote markers are valid after up to three leading
    // spaces and must be followed by whitespace or end-of-line. TODO: support
    // nested quote depth and CommonMark lazy continuation paragraphs.
    int quoteIndent = 0;
    while (quoteIndent < text.size() && quoteIndent < 3 &&
           text.at(quoteIndent) == QLatin1Char(' '))
        ++quoteIndent;
    if (quoteIndent < text.size() &&
        text.at(quoteIndent) == QLatin1Char('>') &&
        (quoteIndent + 1 == text.size() ||
         text.at(quoteIndent + 1).isSpace()))
    {
        int quoteContentStart = quoteIndent + 1;
        if (quoteContentStart < text.size() &&
            (text.at(quoteContentStart) == QLatin1Char(' ') ||
             text.at(quoteContentStart) == QLatin1Char('\\t')))
            ++quoteContentStart;
        formatData->addBlockQuoteFormat(quoteContentStart - 1,
                                        text.size() - 1);
    }

    insertHeadingData(text, index1, formatData);

    // Inline code spans first, so the emphasis parser can skip their
    // content (`` `**literal**` `` must not become bold).
    const QVector<QPair<int, int>> codeSpanRanges =
        parseInlineCodeSpans(text, formatData);

    // CommonMark delimiter-stack emphasis parser (replaces the old
    // three-character window heuristic for *, _, ~~).
    parseEmphasis(text, codeSpanRanges, formatData);

    int index2 = index1 + 1;
    int index3 = index2 + 1;

    int len = text.length();
    while (index1 < len)
    {
        composeSymbolCombination(len, text, index1, index2, index3, test);

        if (test != "")
        {
            // Emphasis (*, _, ~~) is no longer produced by
            // composeSymbolCombination; it is handled by parseEmphasis().

            if (locLink.start != -1)
            {
                if (test == LINK_SYMBOL_URL_END)
                {
                    locLink.end = index1;
                    const QString linkUrl = textLink.mid(
                        locLink.start + 2, (locLink.end - locLink.start - 1));
                    const QString linkTitle =
                        textLink.mid(locLinkTitle.start + 1,
                                     (locLink.start - locLinkTitle.start - 2));
                    insertFormatLinkData(locLinkTitle, locLink, index1, index2,
                                         index3, formatData, test, &linkUrl,
                                         &linkTitle);
                    resetAllLoc();
                }
            }
            else
            {
                if (test == CHECKED_SYMBOL_END && locCheck.start != -1)
                {
                    insertFormatCheckBoxData(locCheck, index1, index2, index3,
                                             formatData,
                                             QString(CHECKED_SYMBOL_END));
                    continue;
                }
                else if (test == UNCHECKED_SYMBOL_END && locCheck.start != -1)
                {
                    insertFormatCheckBoxData(locCheck, index1, index2, index3,
                                             formatData,
                                             QString(UNCHECKED_SYMBOL_END));
                    continue;
                }
                else if (test == CHECK_SYMBOL_START)
                {
                    locCheck.start = index1;
                    incrementIndexes(index1, index2, index3, test.size());
                    continue;
                }
                else if (test == LINK_SYMBOL_TITLE_START)
                {
                    locLinkTitle.start = index1;
                }
                else if (test == LINK_SYMBOL_MID && locLinkTitle.start != -1)
                {
                    locLinkTitle.end = index1;
                    locLink.start = index2;
                }
            }
        }
        incrementIndexes(index1, index2, index3);
    }

    // Images participate in the same per-block fragment collection as
    // emphasis, links, and inline code. Protect image-like text inside code
    // spans while supporting valid destinations with spaces and parentheses.
    const auto imageMatches = parseMarkdownImages(text);
    for (const auto &match : imageMatches)
    {
        bool insideCodeSpan = false;
        for (const auto &range : codeSpanRanges)
        {
            if (match.start >= range.first && match.start <= range.second)
            {
                insideCodeSpan = true;
                break;
            }
        }
        if (insideCodeSpan)
            continue;

        formatData->addImageFormat(match.start, match.end - 1,
                                   match.url, match.alt);
    }

    if (!formatData->isEmpty())
    {
        for (auto it = formatData->formats_begin();
             it != formatData->formats_end(); ++it)
        {
            if ((*it)->getStatus() == FragmentData::LINK_TITLE)
            {
                const QString *linkText =
                    formatData->getLinkUrlFromTitleStart((*it)->getStart());
                const QString *linkTitle =
                    formatData->getLinkTitleFromTitleStart((*it)->getStart());
                if (linkText)
                {
                    formatData->addHiddenFormat(
                        (*it)->getStart(), (*it)->getEnd(),
                        FragmentData::LINK_TITLE, *linkText, *linkTitle);
                }
            }
            else if ((*it)->getStatus() == FragmentData::CHECKED_END ||
                     (*it)->getStatus() == FragmentData::UNCHECKED_END)
            {
                formatData->addHiddenFormat(
                    (*it)->getEnd(), (*it)->getEnd() + 1, (*it)->getStatus(),
                    QString(), QString());
            }
        }
        block.setUserData(formatData);
    }
}

void MkTextDocument::identifyUnicode(QString &line) { Q_UNUSED(line); }

// ---------------------------------------------------------------------------
// ATX headings (H1-H6), including up to three leading spaces and optional
// closing hashes. Marker and closing syntax are hidden only in rendered view.
// ---------------------------------------------------------------------------
void MkTextDocument::insertHeadingData(const QString &text, int &index1,
                                       FormatData *formatData)
{
    int indent = 0;
    while (indent < text.size() && indent < 3 &&
           text.at(indent) == QLatin1Char(' '))
        ++indent;

    int hashStart = indent;
    while (hashStart < text.size() &&
           text.at(hashStart) == QLatin1Char('#'))
        ++hashStart;

    const int headingLevel = hashStart - indent;
    if (headingLevel < 1 || headingLevel > 6)
        return;

    // ATX headings require whitespace or end-of-line after the opening hashes.
    if (hashStart < text.size() &&
        text.at(hashStart) != QLatin1Char(' ') &&
        text.at(hashStart) != QLatin1Char('\t'))
        return;

    QString headingSymbol;
    switch (headingLevel)
    {
    case 1: headingSymbol = QStringLiteral(HEADING1_SYMBOL); break;
    case 2: headingSymbol = QStringLiteral(HEADING2_SYMBOL); break;
    case 3: headingSymbol = QStringLiteral(HEADING3_SYMBOL); break;
    case 4: headingSymbol = QStringLiteral(HEADING4_SYMBOL); break;
    case 5: headingSymbol = QStringLiteral(HEADING5_SYMBOL); break;
    case 6: headingSymbol = QStringLiteral(HEADING6_SYMBOL); break;
    default: return;
    }

    int contentStart = hashStart;
    while (contentStart < text.size() &&
           (text.at(contentStart) == QLatin1Char(' ') ||
            text.at(contentStart) == QLatin1Char('\t')))
        ++contentStart;

    // Mask indentation, hashes, and all spacing after the marker.
    formatData->addFormat(0, contentStart - 1, headingSymbol);
    index1 = contentStart;

    // A closing hash sequence is valid only when preceded by whitespace.
    int contentEnd = text.size() - 1;
    while (contentEnd >= contentStart &&
           (text.at(contentEnd) == QLatin1Char(' ') ||
            text.at(contentEnd) == QLatin1Char('\t')))
        --contentEnd;

    if (contentEnd < contentStart ||
        text.at(contentEnd) != QLatin1Char('#'))
        return;

    int closingStart = contentEnd;
    while (closingStart >= contentStart &&
           text.at(closingStart) == QLatin1Char('#'))
        --closingStart;
    ++closingStart;

    const bool validCloser =
        closingStart == contentStart ||
        text.at(closingStart - 1).isSpace();
    if (!validCloser)
        return;

    int hideStart = closingStart;
    if (hideStart > contentStart && text.at(hideStart - 1).isSpace())
        --hideStart;

    formatData->addMaskRange(hideStart, text.size() - 1);
}

// ---------------------------------------------------------------------------
// Inline code spans.
// ---------------------------------------------------------------------------
QVector<QPair<int, int>>
MkTextDocument::parseInlineCodeSpans(const QString &text,
                                     FormatData *formatData)
{
    QVector<QPair<int, int>> ranges;
    const int n = text.length();
    int i = 0;

    while (i < n)
    {
        if (text.at(i) != QLatin1Char('`'))
        {
            ++i;
            continue;
        }

        int openerLen = 0;
        while (i + openerLen < n && text.at(i + openerLen) == QLatin1Char('`'))
        {
            ++openerLen;
        }

        // Skip fenced code block markers at the start of the line.
        if (i == 0 && openerLen >= 3)
        {
            i += openerLen;
            continue;
        }

        const int openerStart = i;
        const int contentStart = i + openerLen;

        int j = contentStart;
        int closerStart = -1;
        while (j < n)
        {
            if (text.at(j) != QLatin1Char('`'))
            {
                ++j;
                continue;
            }
            int closerLen = 0;
            while (j + closerLen < n &&
                   text.at(j + closerLen) == QLatin1Char('`'))
            {
                ++closerLen;
            }
            if (closerLen == openerLen)
            {
                closerStart = j;
                break;
            }
            j += closerLen;
        }

        if (closerStart < 0)
        {
            // Unmatched backtick - treat as literal text.
            i = contentStart;
            continue;
        }

        const int contentEnd = closerStart - 1;

        formatData->addMaskRange(openerStart, contentStart - 1);
        formatData->addMaskRange(closerStart, closerStart + openerLen - 1);

        if (contentStart <= contentEnd)
        {
            formatData->addFormat(contentStart, contentEnd,
                                  QString(CODE_SPAN_SYMBOL));
            ranges.append(qMakePair(contentStart, contentEnd));
        }

        i = closerStart + openerLen;
    }

    return ranges;
}

// ---------------------------------------------------------------------------
// CommonMark delimiter-stack emphasis parser.
//
// See https://spec.commonmark.org/0.30/#emphasis-and-strong-emphasis
//
// The algorithm:
//   1. Tokenize the block into delimiter runs of '*', '_', or '~~'.
//   2. For each run compute canOpen / canClose.
//      * For '*' and '_' the CommonMark left-flanking / right-flanking rules
//        are used (with the extra intraword rule for '_').
//      * For '~~' GFM flanking rules apply; runs need at least two tildes,
//        and the multiple-of-3 restriction does not apply.
//   3. Walk forward.  When a closer is found, search the opener stack from
//      the top down for a matching opener.  Consume min(opener.remaining,
//      closer.remaining) characters, using 2 if possible (strong) and 1
//      otherwise (emphasis).  Apply the CommonMark "multiple of 3" rule
//      to '*' and '_' only.
//   4. Record each match as an addFormat() call with the appropriate symbol
//      ("*", "**", or "~~").  addFormat() appends the FragmentData and sets
//      the mask bits for the delimiter characters.
// ---------------------------------------------------------------------------
void MkTextDocument::parseEmphasis(
    const QString &text,
    const QVector<QPair<int, int>> &protectedRanges,
    FormatData *formatData)
{
    struct DelimRun
    {
        int pos = 0;
        int len = 0;
        QChar ch;
        bool canOpen = false;
        bool canClose = false;
        int frontUsed = 0;  // chars consumed from the front (as closer)
        int backUsed = 0;   // chars consumed from the back  (as opener)
        int remaining() const { return len - frontUsed - backUsed; }
    };

    const int n = text.length();
    if (n == 0)
        return;

    // --- Step 1: tokenize ---------------------------------------------------
    QVector<DelimRun> runs;
    for (int i = 0; i < n;)
    {
        const QChar c = text.at(i);
        if (c != QLatin1Char('*') && c != QLatin1Char('_') &&
            c != QLatin1Char('~'))
        {
            ++i;
            continue;
        }

        int j = i;
        while (j < n && text.at(j) == c)
            ++j;
        const int runLen = j - i;

        // GFM strikethrough requires at least two tildes; a single tilde
        // is treated as literal text.
        if (c == QLatin1Char('~') && runLen < 2)
        {
            i = j;
            continue;
        }

        // Skip delimiter runs that are entirely inside a code span.
        if (isProtectedPosition(i, protectedRanges))
        {
            i = j;
            continue;
        }

        DelimRun run;
        run.pos = i;
        run.len = runLen;
        run.ch = c;

        const QChar before = (i > 0) ? text.at(i - 1) : QChar();
        const QChar after = (j < n) ? text.at(j) : QChar();

        const bool beforeWs = before.isNull() || before.isSpace();
        const bool afterWs = after.isNull() || after.isSpace();
        const bool beforePunct =
            !before.isNull() && isUnicodePunctuation(before);
        const bool afterPunct = !after.isNull() && isUnicodePunctuation(after);

        const bool leftFlanking =
            !afterWs && (!afterPunct || beforeWs || beforePunct);
        const bool rightFlanking =
            !beforeWs && (!beforePunct || afterWs || afterPunct);

        if (c == QLatin1Char('_'))
        {
            run.canOpen = leftFlanking && (!rightFlanking || beforePunct);
            run.canClose = rightFlanking && (!leftFlanking || afterPunct);
        }
        else if (c == QLatin1Char('~'))
        {
            // GFM strikethrough uses left- and right-flanking rules, requires
            // runs of at least two tildes, and has no multiple-of-3 restriction.
            run.canOpen = leftFlanking;
            run.canClose = rightFlanking;
        }
        else
        {
            // '*' - standard CommonMark flanking rules.
            run.canOpen = leftFlanking;
            run.canClose = rightFlanking;
        }

        runs.append(run);
        i = j;
    }

    // --- Step 2: match openers and closers ----------------------------------
    QVector<int> openerStack;  // indices into runs

    for (int i = 0; i < runs.size(); ++i)
    {
        DelimRun &closer = runs[i];

        if (closer.canClose)
        {
            while (closer.remaining() > 0)
            {
                // Find the nearest matching opener on the stack.
                int matchIdx = -1;
                for (int k = openerStack.size() - 1; k >= 0; --k)
                {
                    const int oi = openerStack[k];
                    DelimRun &opener = runs[oi];
                    if (opener.ch != closer.ch)
                        continue;
                    if (opener.remaining() <= 0)
                        continue;

                    // CommonMark rule 9/10: if either the opener or the
                    // closer can both open and close, the sum of the run
                    // lengths must not be a multiple of 3 unless both are
                    // multiples of 3.  Applies to '*' and '_' only; the
                    // strikethrough rule is not affected.
                    if (opener.ch != QLatin1Char('~'))
                    {
                        const bool openerBoth =
                            opener.canOpen && opener.canClose;
                        const bool closerBoth =
                            closer.canOpen && closer.canClose;
                        if ((openerBoth || closerBoth) &&
                            (opener.len + closer.len) % 3 == 0 &&
                            opener.len % 3 != 0 && closer.len % 3 != 0)
                        {
                            continue;
                        }
                    }

                    matchIdx = k;
                    break;
                }

                if (matchIdx < 0)
                    break;

                const int oi = openerStack[matchIdx];
                DelimRun &opener = runs[oi];

                // Decide how many characters to consume.
                int useCount;
                if (opener.ch == QLatin1Char('~'))
                {
                    if (opener.remaining() < 2 || closer.remaining() < 2)
                        break;
                    useCount = 2;
                }
                else
                {
                    const int avail =
                        qMin(opener.remaining(), closer.remaining());
                    useCount = (avail >= 2) ? 2 : 1;
                }

                // Opener consumes from its back end; closer from its front.
                const int openerCharStart =
                    opener.pos + opener.len - opener.backUsed - useCount;
                const int closerCharStart = closer.pos + closer.frontUsed;

                opener.backUsed += useCount;
                closer.frontUsed += useCount;

                const QString symbol =
                    (opener.ch == QLatin1Char('~'))
                        ? QStringLiteral("~~")
                        : (useCount == 2) ? QStringLiteral("**")
                                          : QStringLiteral("*");

                // addFormat() appends the FragmentData for the content and
                // sets the mask bits for the delimiter characters.
                formatData->addFormat(openerCharStart, closerCharStart, symbol);

                if (opener.remaining() <= 0)
                    openerStack.remove(matchIdx);
            }
        }

        // A run that can open and still has characters left becomes an
        // opener for future closers.
        if (closer.canOpen && closer.remaining() > 0)
            openerStack.append(i);
    }
}

void MkTextDocument::insertFormatData(FormatLocation &loc, int &index1,
                                      int &index2, int &index3,
                                      FormatData *formatData,
                                      const QString &test)
{
    if (loc.start == -1)
    {
        loc.start = index1;
    }
    else
    {
        loc.end = index1;
        if (loc.end - loc.start > 1)
        {
            loc.start = (loc.start < 1) ? 0 : loc.start;
            formatData->addFormat(loc.start, loc.end, test);
            loc.reset();
            resetAllLoc();
        }
    }
    incrementIndexes(index1, index2, index3, test.size());
}

void MkTextDocument::insertFormatCheckBoxData(FormatLocation &loc, int &index1,
                                              int &index2, int &index3,
                                              FormatData *formatData,
                                              const QString &test)
{
    loc.end = index3;
    if (loc.end - loc.start == 5)
    {
        formatData->addFormat(loc.start, loc.end, test);
        loc.reset();
    }
    incrementIndexes(index1, index2, index3, test.size());
}

void MkTextDocument::insertFormatLinkData(
    FormatLocation &locTitle, FormatLocation &locLink, int &index1, int &index2,
    int &index3, FormatData *formatData, const QString &test,
    const QString *linkUrl, const QString *linkTitle)
{
    locLink.end = locLink.start + 2 + linkUrl->length();
    if (locLink.end - locLink.start > 3)
    {
        formatData->addFormat(locTitle.start, locTitle.end,
                              QString(LINK_SYMBOL_TITLE_END), linkUrl,
                              linkTitle);
        formatData->addFormat(locLink.start, locLink.end,
                              QString(LINK_SYMBOL_URL_END), linkUrl, linkTitle);
        locLink.reset();
        locTitle.reset();
    }
    incrementIndexes(index1, index2, index3, test.size());
}

void MkTextDocument::incrementIndexes(int &index1, int &index2, int &index3,
                                      const int size)
{
    index1 += size;
    index2 += size;
    index3 += size;
}

bool MkTextDocument::convertCharacterToSymbol(const QChar &single,
                                              QString &text)
{
    if (single == '*' || single == '_' || single == '~')
    {
        text += single;
        return true;
    }
    return false;
}

void MkTextDocument::convertCharacterToCheckboxSymbol(const QChar &single,
                                                      QString &text)
{
    if (single == '-' || single == ' ' || single == '[' || single == ']' ||
        single == 'x')
        text += single;
}

bool MkTextDocument::convertCharacterToLinkSymbol(const QChar &single,
                                                  QString &text)
{
    if (single == '[' || single == ']' || single == '(' || single == ')' ||
        single == '<' || single == '>')
    {
        text += single;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Only checkbox and link markers are detected here.  Emphasis (*, _, ~~) is
// handled by parseEmphasis(); the old three-character window heuristic for
// emphasis has been removed.
// ---------------------------------------------------------------------------
void MkTextDocument::composeSymbolCombination(int length, const QString &text,
                                              int &index1, int &index2,
                                              int &index3, QString &result)
{
    result.clear();
    if (index3 < length)
    {
        convertCharacterToCheckboxSymbol(text[index1], result);
        convertCharacterToCheckboxSymbol(text[index2], result);
        convertCharacterToCheckboxSymbol(text[index3], result);

        if (result == CHECK_SYMBOL_START || result == CHECKED_SYMBOL_END ||
            result == UNCHECKED_SYMBOL_END)
        {
            return;
        }
    }

    result.clear();
    if (index3 < length)
    {
        convertCharacterToLinkSymbol(text[index1], result);
        convertCharacterToLinkSymbol(text[index2], result);
        convertCharacterToLinkSymbol(text[index3], result);
        if (result == LINK_SYMBOL_MID || result == LINK_SYMBOL_URL_START ||
            result == LINK_SYMBOL_URL_END || result == ")>)")
        {
            if (result == ")>)")
                result = ">)";
            return;
        }
    }

    result.clear();
    if (convertCharacterToLinkSymbol(text[index1], result))
        return;
}

void MkTextDocument::composeOnlyLinkSymbolCombination(int length,
                                                      const QString &text,
                                                      int &index1, int &index2,
                                                      int &index3,
                                                      QString &result)
{
    result.clear();
    if (index3 < length)
    {
        convertCharacterToLinkSymbol(text[index1], result);
        convertCharacterToLinkSymbol(text[index2], result);
        convertCharacterToLinkSymbol(text[index3], result);
        if (result == LINK_SYMBOL_MID || result == LINK_SYMBOL_URL_START ||
            result == LINK_SYMBOL_URL_END || result == ")>)")
        {
            if (result == ")>)")
                result = ">)";
            return;
        }
    }
    result.clear();
    if (convertCharacterToLinkSymbol(text[index1], result))
        return;
}

void MkTextDocument::setCodeBlockMargin(QTextBlock &block, int leftMargin,
                                        int rightMargin, int topMargin,
                                        int bottomMargin)
{
    QTextCursor cursor(block);
    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setLeftMargin(leftMargin);
    blockFormat.setRightMargin(rightMargin);

    if (topMargin)
    {
        QString content = block.text().mid(3);
        if (!content.isEmpty())
        {
            blockFormat.setTopMargin(topMargin);
            blockFormat.setBottomMargin(topMargin * 2 / 3);
        }
        QTextCursor previousBlockCursor(block.previous());
        if (block.isValid())
        {
            QTextBlockFormat previousBlockFormat =
                previousBlockCursor.blockFormat();
            previousBlockFormat.setLineHeight(
                topMargin, QTextBlockFormat::LineDistanceHeight);
            previousBlockCursor.setBlockFormat(previousBlockFormat);
        }
        block.next();
    }

    if (bottomMargin)
        blockFormat.setBottomMargin(bottomMargin);

    cursor.setBlockFormat(blockFormat);
}

void MkTextDocument::resetTextBlockFormat(QTextBlock block)
{
    QTextCursor cursor(block);
    QTextCharFormat format;
    cursor.setPosition(block.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.setCharFormat(format);

    // Reset previous heading spacing before reformatting the line. If it is
    // still a heading, applyMkFormat() will apply the matching style again.
    FormatData *data = dynamic_cast<FormatData *>(block.userData());
    bool isHeading = false;
    if (data)
    {
        for (auto it = data->formats_begin(); it != data->formats_end(); ++it)
        {
            const auto status = (*it)->getStatus();
            if (status == FragmentData::HEADING1 ||
                status == FragmentData::HEADING2 ||
                status == FragmentData::HEADING3 ||
                status == FragmentData::HEADING4 ||
                status == FragmentData::HEADING5 ||
                status == FragmentData::HEADING6 ||
                status == FragmentData::BLOCKQUOTE)
            {
                isHeading = true;
                break;
            }
        }
    }

    if (isHeading)
    {
        QTextBlockFormat defaultBlockFormat;
        cursor.setBlockFormat(defaultBlockFormat);
    }
}

void MkTextDocument::applyMkFormat(QTextBlock &block,
                                   const BlockSupport &span,
                                   FragmentData::FormatSymbol status,
                                   FormatCollection &formatCollection)
{
    const int start = span.getStart();
    int end = span.getEnd();
    QTextCharFormat *format = nullptr;
    int headingLevel = 0;

    switch (status)
    {
    case FragmentData::BOLD:
        format = formatCollection.getBold();
        break;
    case FragmentData::ITALIC:
        format = formatCollection.getItalic();
        break;
    case FragmentData::STRIKETHROUGH:
        format = formatCollection.getStrikethrough();
        break;
    case FragmentData::HEADING1:
        format = formatCollection.getHeading1();
        headingLevel = 1;
        end = block.length() - 1;
        break;
    case FragmentData::HEADING2:
        format = formatCollection.getHeading2();
        headingLevel = 2;
        end = block.length() - 1;
        break;
    case FragmentData::HEADING3:
        format = formatCollection.getHeading3();
        headingLevel = 3;
        end = block.length() - 1;
        break;
    case FragmentData::HEADING4:
        format = formatCollection.getHeading4();
        headingLevel = 4;
        end = block.length() - 1;
        break;
    case FragmentData::HEADING5:
        format = formatCollection.getHeading5();
        headingLevel = 5;
        end = block.length() - 1;
        break;
    case FragmentData::HEADING6:
        format = formatCollection.getHeading6();
        headingLevel = 6;
        end = block.length() - 1;
        break;
    case FragmentData::CHECKED_END:
    case FragmentData::UNCHECKED_END:
        break;
    case FragmentData::LINK_TITLE:
        format = formatCollection.getLink();
        break;
    case FragmentData::CODE_SPAN:
        format = formatCollection.getCodeSpan();
        break;
    case FragmentData::BLOCKQUOTE:
    {
        QTextCharFormat quoteFormat;
        quoteFormat.setForeground(QColor(QStringLiteral("#65727C")));
        quoteFormat.setFontItalic(true);

        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setLeftMargin(18);
        blockFormat.setBackground(QColor(90, 120, 150, 20));
        QTextCursor blockCursor(block);
        blockCursor.setBlockFormat(blockFormat);
        format = &quoteFormat;
        start = 0;
        end = block.length() - 1;
        break;
    }
    case FragmentData::IMAGE:
    {
        // Replace the complete Markdown image expression with a QText image
        // object. The rawDocument retains the original Markdown for editing.
        FormatData *data = dynamic_cast<FormatData *>(block.userData());
        if (!data)
            return;

        const QString destination = data->getImageUrl(start);
        if (destination.isEmpty())
            return;

        QTextImageFormat imageFormat;
        imageFormat.setName(destination);
        imageFormat.setToolTip(data->getImageAltText(start));

        // TODO: provide asynchronous network loading for HTTP(S) destinations.
        // QTextDocument can render registered/local resources, but remote image
        // fetching needs an explicit resource provider and failure handling.
        QTextCursor imageCursor(this);
        imageCursor.setPosition(block.position() + start);
        imageCursor.setPosition(
            block.position() + qMin(end + 1, block.length() - 1),
            QTextCursor::KeepAnchor);
        imageCursor.insertImage(imageFormat);
        return;
    }
    default:
        break;
    }

    if (format == nullptr)
        return;

    if (headingLevel > 0)
    {
        // Clear, consistent vertical rhythm makes headings read as document
        // structure rather than merely larger inline text.
        static const qreal topMargins[] = {0, 12, 10, 8, 6, 5, 4};
        static const qreal bottomMargins[] = {0, 8, 7, 6, 4, 3, 2};
        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setTopMargin(topMargins[headingLevel]);
        blockFormat.setBottomMargin(bottomMargins[headingLevel]);
        QTextCursor blockCursor(block);
        blockCursor.setBlockFormat(blockFormat);
    }

    // FragmentData ranges use inclusive character indexes, while QTextCursor
    // selections use an exclusive end position. Convert the end index once,
    // and clamp it to the end of the block's text (before the paragraph mark).
    const int startPoint = block.position() + start;
    const int endPoint =
        block.position() + qMin(end + 1, block.length() - 1);

    QTextCursor cursor(this);
    cursor.setPosition(startPoint);
    cursor.setPosition(endPoint, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(*format);
}

void MkTextDocument::applyCheckBoxLinkEffect(FormatData *data,
                                             QTextBlock &block,
                                             QTextCursor &cursor)
{
    for (QVector<FragmentData *>::Iterator it = data->hiddenFormats_begin();
         it < data->hiddenFormats_end(); it++)
    {
        if ((*it)->getStatus() == FragmentData::CHECKED_END ||
            (*it)->getStatus() == FragmentData::UNCHECKED_END)
        {
            const QPair<int, int> checkPos(block.blockNumber(),
                                           (*it)->getStart());
            if (!checkMarkPositions.contains(checkPos))
                checkMarkPositions.append(checkPos);

            cursor.setPosition(block.position() + (*it)->getStart());
            cursor.movePosition(QTextCursor::NextCharacter,
                                QTextCursor::KeepAnchor);

            if ((*it)->getStatus() == FragmentData::CHECKED_END)
                cursor.insertText(CHECKED_PIC);
            else
                cursor.insertText(UNCHECKED_PIC);
        }
        else if ((*it)->getStatus() == FragmentData::LINK_TITLE)
        {
            const std::tuple<int, int, int, const QString *> linkPos(
                block.blockNumber(), (*it)->getStart(), (*it)->getEnd(),
                data->getHiddenTitle((*it)->getStart()));
            if (!linkPositions.contains(linkPos))
                linkPositions.append(linkPos);
        }
    }
}

void MkTextDocument::hideSymbols(QTextBlock &block, const QString &symbol)
{
    QString textBlock = block.text();
    QRegularExpression regex(symbol);
    textBlock.replace(regex, "");

    QTextCursor editCursor(block);
    editCursor.movePosition(QTextCursor::StartOfBlock);
    editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editCursor.insertText(textBlock);
}

void MkTextDocument::hideSymbolsAtPos(QString &text, int pos,
                                      const QString &symbol)
{
    text.remove(pos, symbol.length());
    if (symbol == CHECKED_SYMBOL_END)
        text.insert(pos, CHECKED_PIC);
    else if (symbol == UNCHECKED_SYMBOL_END)
        text.insert(pos, UNCHECKED_PIC);
}

void MkTextDocument::showSymbols(QTextBlock &block, const QString &symbol)
{
    QRegularExpression regex(symbol);
    QRegularExpressionMatch matchCodeBlock = regex.match(block.text());
    if (matchCodeBlock.hasMatch())
        return;

    QTextCursor editCursor(block);
    QString newText = symbol + block.text();
    editCursor.movePosition(QTextCursor::StartOfBlock);
    editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editCursor.insertText(newText);
}

void MkTextDocument::removeCheckBoxLinkMousePosition(QTextBlock &block,
                                                     FormatData *formatData,
                                                     SelectRange *range)
{
    Q_UNUSED(formatData);
    Q_UNUSED(range);

    const int blockNo = block.blockNumber();

    for (int i = checkMarkPositions.size() - 1; i >= 0; --i)
    {
        if (checkMarkPositions[i].first == blockNo)
            checkMarkPositions.removeAt(i);
    }

    auto newEnd = std::remove_if(
        linkPositions.begin(), linkPositions.end(),
        [blockNo](const std::tuple<int, int, int, const QString *> &linkPos)
        { return std::get<0>(linkPos) == blockNo; });
    linkPositions.erase(newEnd, linkPositions.end());
}

void MkTextDocument::showSymbolsAtPos(QString &text, int pos,
                                      const QString &symbol)
{
    if (symbol == CHECKED_SYMBOL_END || symbol == UNCHECKED_SYMBOL_END)
        text.remove(pos, 1);
    text.insert(pos, symbol);
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

void MkTextDocument::enterKeyPressedHandle(int blockNumber, int &newCursorPos)
{
    numberListDetect(blockNumber, newCursorPos);
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

void MkTextDocument::saveRawDocumentHandler()
{
    this->rawDocument.setPlainText(this->toPlainText());
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
    FormatCollection formatCollection(fontSize);

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
                resetTextBlockFormat(block);
                continue;
            }

            BlockData *blockData = dynamic_cast<BlockData *>(data);
            if (blockData)
            {
                resetTextBlockFormat(block);
                showHideCodeBlock(blockData, true, fontSize);
                if (blockData->getStatus() == BlockData::content)
                    setCodeBlockMargin(block, fontSize * 10 / 3, fontSize, 0);
                continue;
            }

            LineData *lineData = dynamic_cast<LineData *>(data);
            if (lineData && !lineData->isHidden())
            {
                resetTextBlockFormat(block);
                lineData->setHidden(true);
                lineData->setDraw(true);
                hideSymbols(block, lineData->getSymbol());
                continue;
            }

            FormatData *formatData = dynamic_cast<FormatData *>(data);
            if (formatData && !formatData->isHidden())
            {
                formatData->setHidden(true);

                if (!formatData->isFormatted())
                {
                    for (QVector<FragmentData *>::Iterator it =
                             formatData->formats_begin();
                         it < formatData->formats_end(); it++)
                    {
                        applyMkFormat(block, *(*it),
                                      (*it)->getStatus(), formatCollection);
                    }
                }

                const QBitArray &mask(formatData->getMask());
                if (block.text().size() != mask.size())
                    continue;

                QTextCursor cursor(block);
                cursor.movePosition(QTextCursor::EndOfBlock);
                for (int index = mask.size() - 1; index >= 0; --index)
                {
                    if (mask[index])
                        cursor.deletePreviousChar();
                    else
                        cursor.movePosition(QTextCursor::PreviousCharacter);
                }

                applyCheckBoxLinkEffect(formatData, block, cursor);
            }
        }
    }

    foreach (int num, hiddenBlocks) range->hideBlocks.erase(num);

    emit connectCurosPos();
}

void MkTextDocument::showMKSymbolsFromCurrentSelectedBlocks(
    SelectRange *const range)
{
    emit disconnectCursorPos();
    int fontSize = this->defaultFont().pointSize();
    FormatCollection formatCollection(fontSize);
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
                resetTextBlockFormat(block);
                continue;
            }

            BlockData *blockData = dynamic_cast<BlockData *>(data);
            if (blockData)
            {
                resetTextBlockFormat(block);
                showHideCodeBlock(blockData, false, fontSize);
                if (blockData->getStatus() == BlockData::content)
                    setCodeBlockMargin(block, fontSize * 10 / 3, fontSize, 0);
                continue;
            }

            LineData *lineData = dynamic_cast<LineData *>(data);
            if (lineData && lineData->isHidden())
            {
                lineData->setHidden(false);
                lineData->setDraw(false);
                showSymbols(block, lineData->getSymbol());
                continue;
            }

            FormatData *formatData = dynamic_cast<FormatData *>(data);
            if (formatData)
            {
                bool isPosInBlockAtMax =
                    (range->currentposInBlock == (block.length() - 1));
                bool isHidden = formatData->isHidden();
                resetTextBlockFormat(block);
                removeCheckBoxLinkMousePosition(block, formatData, range);

                QTextCursor cursor(block);
                cursor.movePosition(QTextCursor::StartOfBlock);
                cursor.movePosition(QTextCursor::EndOfBlock,
                                    QTextCursor::KeepAnchor);
                QString line =
                    rawDocument.findBlockByNumber(block.blockNumber()).text();
                cursor.insertText(line);
                identifyFormatData(block);

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
                            applyMkFormat(block, *(*it), (*it)->getStatus(),
                                          formatCollection);
                    }

                    // Render images right-to-left so replacing one image cannot
                    // invalidate the source offsets of images earlier in the line.
                    for (auto it = newData->formats_rbegin();
                         it != newData->formats_rend(); ++it)
                    {
                        if ((*it)->getStatus() == FragmentData::IMAGE)
                            applyMkFormat(block, *(*it), (*it)->getStatus(),
                                          formatCollection);
                    }

                    if (isHidden)
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

    emit connectCurosPos();
}

void MkTextDocument::showHideCodeBlock(BlockData *data, bool hide, int fontSize)
{
    QTextBlock startCode = this->findBlockByNumber(data->getStartBlock());
    QTextBlock endCode = this->findBlockByNumber(data->getEndBlock());

    if (hide)
    {
        BlockData *startBlockData =
            dynamic_cast<BlockData *>(startCode.userData());
        if (startBlockData)
        {
            setCodeBlockMargin(startCode, fontSize * 7 / 4, fontSize, fontSize);
            if (!startBlockData->isHidden())
            {
                hideSymbols(startCode, CODEBLOCK_SYMBOL);
                startBlockData->setHidden(true);
            }
        }
        BlockData *endBlockData = dynamic_cast<BlockData *>(endCode.userData());
        if (endBlockData)
        {
            setCodeBlockMargin(endCode, fontSize * 7 / 4, fontSize, 0,
                               fontSize);
            if (!endBlockData->isHidden())
            {
                hideSymbols(endCode, CODEBLOCK_SYMBOL);
                endBlockData->setHidden(true);
            }
        }
    }
    else
    {
        BlockData *startBlockData =
            dynamic_cast<BlockData *>(startCode.userData());
        if (startBlockData && startBlockData->isHidden())
        {
            setCodeBlockMargin(startCode, fontSize * 7 / 4, fontSize, fontSize);
            if (startBlockData->isHidden())
            {
                showSymbols(startCode, CODEBLOCK_SYMBOL);
                startBlockData->setHidden(false);
            }
        }
        BlockData *endBlockData = dynamic_cast<BlockData *>(endCode.userData());
        if (endBlockData && endBlockData->isHidden())
        {
            showSymbols(endCode, CODEBLOCK_SYMBOL);
            if (endBlockData->isHidden())
            {
                endBlockData->setHidden(false);
                setCodeBlockMargin(endCode, fontSize * 7 / 4, fontSize, 0,
                                   fontSize);
            }
        }
    }
}

void MkTextDocument::pushCheckBoxHandle(const int position)
{
    QTextBlock block = findBlock(position);
    int blockNo = block.blockNumber();
    QTextBlockUserData *data = block.userData();
    FormatData *formatData = dynamic_cast<FormatData *>(data);

    if (!formatData)
        return;

    int renderedPosInBlock = position - block.position();
    int rawPosInBlock = renderedPosInBlock;
    const QBitArray &mask = formatData->getMask();
    for (int x = 0; x <= rawPosInBlock; ++x)
    {
        if (mask[x])
            ++rawPosInBlock;
    }

    int startOfCheckBoxInBlock = rawPosInBlock - 5;
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

void MkTextDocument::undoStackPush(QUndoCommand *edit) { undoStack.push(edit); }

void MkTextDocument::undoStackUndo(bool &success)
{
    if (undoStack.canUndo())
    {
        undoStack.undo();
        success = true;
    }
}

void MkTextDocument::undoStackRedo(bool &success)
{
    if (undoStack.canRedo())
    {
        undoStack.redo();
        success = true;
    }
}

void MkTextDocument::resetFormatLocation()
{
    locBoldA.reset();
    locBoldU.reset();
    locItalicA.reset();
    locItalicU.reset();
    locStrike.reset();
    locCheck.reset();
    locHeading1.reset();
    locHeading2.reset();
    locHeading3.reset();
    locLink.reset();
    locLinkTitle.reset();
}

void MkTextDocument::numberListDetect(int blockNumber, int &newCursorPosition)
{
    QTextCursor cursor(this->findBlockByNumber(blockNumber));
    QTextBlock currentBlock = this->findBlockByNumber(blockNumber - 1);
    QString lineText = currentBlock.text();

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

EditCommand::EditCommand(UndoData &data)
{
    this->view = data.view;
    this->viewEditTypeStore = data.viewEditTypeStore;
    this->doc = dynamic_cast<MkTextDocument *>(data.doc);
    this->scrollValue = data.scrollValue;
    isConstructorRedo = true;
    this->undoSelectRange = data.oldSelectRange;
    this->viewSelectRangeStore = data.viewSelectRangeStore;
    this->blockNo = data.blockNo;
    this->posInBlock = data.posInBlock;
    this->editType = data.editType;
    this->undoSelectRange.scrollValue = data.scrollValue;

    switch (editType)
    {
    case undoRedo:
        return;

    case singleEdit:
        this->undoText = data.oldBlock;
        {
            const QTextBlock viewBlock =
                data.doc->findBlockByNumber(data.blockNo);
            this->redoText = viewBlock.isValid()
                                 ? viewBlock.text()
                                 : QString();

            // A rendered image is represented by U+FFFC in QTextBlock::text().
            // In that case, use the raw Markdown snapshot so redo preserves
            // the original ![alt](path) expression. For ordinary text edits,
            // the view snapshot is necessary because some callers/tests may
            // not have wired saveSingleRawBlock yet.
            if (doc && this->redoText.contains(QChar::ObjectReplacementCharacter))
            {
                QTextBlock sourceBlock =
                    doc->getRawDocument()->findBlockByNumber(data.blockNo);
                if (sourceBlock.isValid())
                    this->redoText = sourceBlock.text();
            }
        }
        this->undoSelectRange.isCheckBox = false;
        break;

    case multiDelete:
    case checkbox:
    case enterPressed:
    case multiEdit:
        this->undoText = data.oldText;
        this->redoText = (doc ? doc->getRawDocument()->toPlainText()
                              : data.doc->toPlainText());
        break;
    }
}

void EditCommand::undo()
{
    switch (editType)
    {
    case undoRedo:
        return;

    case singleEdit:
        doc->setUndoRedoText(undoSelectRange.currentBlockNo, this->undoText);
        break;

    case multiDelete:
    case checkbox:
    case enterPressed:
    case multiEdit:
        doc->setUndoRedoText(undoText);
        break;
    }

    *viewEditTypeStore = editType;
    *viewSelectRangeStore = undoSelectRange;
}

void EditCommand::redo()
{
    if (isConstructorRedo)
    {
        isConstructorRedo = false;
        return;
    }

    switch (editType)
    {
    case undoRedo:
        return;

    case singleEdit:
        doc->setUndoRedoText(this->blockNo, this->redoText);
        break;

    case multiDelete:
    case checkbox:
    case enterPressed:
    case multiEdit:
        doc->setUndoRedoText(redoText);
        break;
    }

    *this->viewEditTypeStore = editType;

    viewSelectRangeStore->hasSelection = false;
    viewSelectRangeStore->currentBlockNo = this->blockNo;
    viewSelectRangeStore->currentposInBlock = this->posInBlock;
    viewSelectRangeStore->isCheckBox = this->undoSelectRange.isCheckBox;
    viewSelectRangeStore->scrollValue = this->undoSelectRange.scrollValue;
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

    showMKSymbolsFromCurrentSelectedBlocks(&this->selectRange);
}