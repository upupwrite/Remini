#include "markdownrenderer.h"

#include "blockrenderer.h"
#include "linedata.h"

#include <QColor>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextImageFormat>

namespace {

// Builds the replacement object for one inline image. Returns an empty format
// (empty name) when the destination is missing so callers can bail out.
QTextImageFormat makeImageFormat(FormatData *data, int imageStart)
{
    QTextImageFormat imageFormat;
    if (!data)
        return imageFormat;

    const QString destination = data->getImageUrl(imageStart);
    if (destination.isEmpty())
        return imageFormat;

    imageFormat.setName(destination);
    imageFormat.setToolTip(data->getImageAltText(imageStart));
    return imageFormat;
}

}  // namespace

void MarkdownRenderer::setCodeBlockMargin(QTextBlock &block, int leftMargin,
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

void MarkdownRenderer::resetTextBlockFormat(QTextBlock block)
{
    QTextCursor cursor(block);

    // Table rows carry tab stops and a collapsed delimiter height that must not
    // leak into the editable (raw) view.
    if (dynamic_cast<TableData *>(block.userData()))
        cursor.setBlockFormat(QTextBlockFormat());

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

void MarkdownRenderer::applyMkFormat(QTextDocument *document,
                                     QTextBlock &block,
                                     const BlockSupport &span,
                                     FragmentData::FormatSymbol status,
                                     FormatCollection &formatCollection)
{
    int start = span.getStart();
    int end = span.getEnd();
    QTextCharFormat *format = nullptr;
    // Keep the quote format alive across the switch to avoid a dangling pointer.
    QTextCharFormat quoteFormat;
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
        quoteFormat.setForeground(formatCollection.quoteText);
        quoteFormat.setFontItalic(true);

        // Nested quotes indent further; a marker-only line still reports a
        // depth of one so it keeps the same visual treatment.
        int quoteDepth = 1;
        if (FormatData *data = dynamic_cast<FormatData *>(block.userData()))
            quoteDepth = qMax(1, data->getBlockQuoteDepth());

        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setLeftMargin(18 * quoteDepth);
        blockFormat.setBackground(formatCollection.quoteBackground);
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
        // object. The raw document retains the original Markdown for editing.
        FormatData *data = dynamic_cast<FormatData *>(block.userData());
        if (!data)
            return;

        const QTextImageFormat imageFormat = makeImageFormat(data, start);
        if (imageFormat.name().isEmpty())
            return;

        QTextCursor imageCursor(document);
        const BlockRenderRange imageRange(start, end);
        if (!BlockRenderer::selectRange(imageCursor, block, imageRange))
            return;
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

    // The shared renderer owns inclusive-to-exclusive conversion, bounds
    // checking, and empty-block handling for all character-format spans.
    const BlockRenderRange renderRange(start, end);
    BlockRenderer::applyCharacterFormat(document, block, renderRange, *format);
}

void MarkdownRenderer::applyCheckBoxLinkEffect(
    FormatData *data, QTextBlock &block,
    QVector<QPair<int, int>> &checkMarkPositions,
    QVector<std::tuple<int, int, int, const QString *>> &linkPositions)
{
    QTextCursor cursor(block);

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

void MarkdownRenderer::hideSymbols(QTextBlock &block, const QString &symbol)
{
    QString textBlock = block.text();
    QRegularExpression regex(symbol);
    textBlock.replace(regex, "");

    QTextCursor editCursor(block);
    editCursor.movePosition(QTextCursor::StartOfBlock);
    editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editCursor.insertText(textBlock);
}

void MarkdownRenderer::hideSymbolsAtPos(QString &text, int pos,
                                        const QString &symbol)
{
    text.remove(pos, symbol.length());
    if (symbol == CHECKED_SYMBOL_END)
        text.insert(pos, CHECKED_PIC);
    else if (symbol == UNCHECKED_SYMBOL_END)
        text.insert(pos, UNCHECKED_PIC);
}

void MarkdownRenderer::showSymbols(QTextBlock &block, const QString &symbol)
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

void MarkdownRenderer::removeCheckBoxLinkMousePosition(
    QTextBlock &block,
    QVector<QPair<int, int>> &checkMarkPositions,
    QVector<std::tuple<int, int, int, const QString *>> &linkPositions)
{
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

void MarkdownRenderer::showSymbolsAtPos(QString &text, int pos,
                                        const QString &symbol)
{
    if (symbol == CHECKED_SYMBOL_END || symbol == UNCHECKED_SYMBOL_END)
        text.remove(pos, 1);
    text.insert(pos, symbol);
}

void MarkdownRenderer::showHideCodeBlock(QTextDocument *document,
                                         BlockData *data, bool hide,
                                         int fontSize)
{
    QTextBlock startCode = document->findBlockByNumber(data->getStartBlock());
    QTextBlock endCode = document->findBlockByNumber(data->getEndBlock());

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

void MarkdownRenderer::insertImages(QTextDocument *document, QTextBlock &block,
                                    FormatData *data)
{
    if (!document || !data || !block.isValid())
        return;

    const QVector<QPair<int, int>> spans = data->imageSpans();
    if (spans.isEmpty())
        return;

    const QBitArray &mask = data->getMask();

    // Walk the images from the end of the line towards the start. The block
    // text currently holds the source line with masked characters removed but
    // the image expressions still intact, so an image starts at
    // (source start - masked characters before it).
    for (int k = spans.size() - 1; k >= 0; --k)
    {
        const int sourceStart = spans.at(k).first;
        const int sourceEnd = spans.at(k).second;

        const QTextImageFormat imageFormat =
            makeImageFormat(data, sourceStart);
        if (imageFormat.name().isEmpty())
            continue;

        int maskedBefore = 0;
        for (int i = 0; i < sourceStart && i < mask.size(); ++i)
        {
            if (mask.testBit(i))
                ++maskedBefore;
        }

        const int textStart = sourceStart - maskedBefore;
        const int textLength = sourceEnd - sourceStart + 1;
        const int textLimit = block.length() - 1;

        if (textStart < 0 || textStart >= textLimit ||
            textStart + textLength > textLimit)
            continue;

        QTextCursor cursor(document);
        cursor.setPosition(block.position() + textStart);
        cursor.setPosition(block.position() + textStart + textLength,
                           QTextCursor::KeepAnchor);
        cursor.insertImage(imageFormat);
    }
}

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------
void MarkdownRenderer::applyTableRowFormat(QTextDocument *document,
                                           QTextBlock &block,
                                           TableData *table,
                                           FormatCollection &formatCollection)
{
    Q_UNUSED(document);
    Q_UNUSED(formatCollection);

    if (!table || !block.isValid())
        return;

    QTextCursor cursor(block);
    QTextBlockFormat blockFormat = cursor.blockFormat();

    // Shared tab stops make every row line up in the same columns.
    const QVector<qreal> &stops = table->getTabStops();
    QList<QTextOption::Tab> tabs;
    tabs.reserve(stops.size());
    for (const qreal stop : stops)
        tabs.append(QTextOption::Tab(stop, QTextOption::LeftTab));
    blockFormat.setTabPositions(tabs);

    blockFormat.setLeftMargin(0);
    blockFormat.setRightMargin(0);

    if (table->getRowKind() == TableData::Delimiter)
    {
        // The delimiter row only carries the header separator line, so keep it
        // as a thin strip instead of an empty text line.
        blockFormat.setTopMargin(0);
        blockFormat.setBottomMargin(0);
        blockFormat.setLineHeight(2, QTextBlockFormat::FixedHeight);
        cursor.setBlockFormat(blockFormat);
        return;
    }

    blockFormat.setTopMargin(table->getRowKind() == TableData::Header ? 4 : 0);
    blockFormat.setBottomMargin(table->getRowKind() == TableData::Header ? 2 : 0);
    cursor.setBlockFormat(blockFormat);

    if (table->getRowKind() != TableData::Header)
        return;

    // Bold header row. Cells are separated by tabs, so a single format over the
    // whole line is enough.
    QTextCursor headerCursor(document);
    headerCursor.setPosition(block.position());
    headerCursor.setPosition(block.position() + qMax(0, block.length() - 1),
                             QTextCursor::KeepAnchor);
    QTextCharFormat headerFormat;
    headerFormat.setFontWeight(QFont::DemiBold);
    headerCursor.mergeCharFormat(headerFormat);
}

void MarkdownRenderer::resetTableRowFormat(QTextBlock &block)
{
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.setBlockFormat(QTextBlockFormat());
}
