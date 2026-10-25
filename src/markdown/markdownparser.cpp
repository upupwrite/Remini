#include "markdownparser.h"

#include "formatdata.h"
#include "tabledata.h"

#include <QtGlobal>

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------
namespace {

// CommonMark defines "Unicode punctuation" as any character in the Unicode
// P (punctuation) or S (symbol) categories. Qt exposes both directly.
bool isUnicodePunctuation(QChar c)
{
    if (c.isNull())
        return false;
    return c.isPunct() || c.isSymbol();
}

bool isMarkdownEscaped(const QString &text, int position)
{
    int backslashes = 0;
    for (int i = position - 1; i >= 0 && text.at(i) == QLatin1Char('\\'); --i)
        ++backslashes;
    return (backslashes % 2) != 0;
}

}  // namespace

// A delimiter run is "protected" if it sits entirely inside one of the ranges
// claimed by inline code spans, links, or escaped characters. Protected runs
// are skipped by the emphasis and marker scanners.
bool MarkdownParser::isProtectedPosition(
    int pos, const QVector<QPair<int, int>> &ranges)
{
    for (const auto &r : ranges)
    {
        if (pos >= r.first && pos <= r.second)
            return true;
    }
    return false;
}

int MarkdownParser::protectedRangeEndAt(
    int pos, const QVector<QPair<int, int>> &ranges)
{
    int end = -1;
    for (const auto &r : ranges)
    {
        if (pos >= r.first && pos <= r.second)
            end = qMax(end, r.second);
    }
    return end;
}

// ---------------------------------------------------------------------------
// Blockquote markers
// ---------------------------------------------------------------------------
int MarkdownParser::blockQuoteMarkerEnd(const QString &text, int &depth)
{
    depth = 0;
    int i = 0;
    while (i < text.size() && i < 3 && text.at(i) == QLatin1Char(' '))
        ++i;

    bool sawMarker = false;
    while (i < text.size() && text.at(i) == QLatin1Char('>'))
    {
        sawMarker = true;
        ++depth;
        ++i;
        // A single space after a marker is consumed; any further spaces are
        // content. This matches CommonMark's "marker + space" rule and lets
        // "> >" and ">>" nest correctly.
        if (i < text.size() && text.at(i) == QLatin1Char(' '))
            ++i;
    }

    return sawMarker ? i : -1;
}

QString MarkdownParser::blockQuotePrefix(const QString &text)
{
    int depth = 0;
    const int contentStart = blockQuoteMarkerEnd(text, depth);
    if (contentStart < 0)
        return QString();
    return text.left(contentStart);
}

// ---------------------------------------------------------------------------
// Backslash escapes
// ---------------------------------------------------------------------------
QVector<QPair<int, int>> MarkdownParser::maskEscapedCharacters(
    const QString &text, FormatData *formatData)
{
    QVector<QPair<int, int>> escaped;
    const int n = text.size();
    int i = 0;
    while (i < n - 1)
    {
        if (text.at(i) != QLatin1Char('\\'))
        {
            ++i;
            continue;
        }

        // Count the whole backslash run so that `\\\\*` stays literal while
        // `\\*` escapes the asterisk.
        int runLength = 0;
        while (i + runLength < n &&
               text.at(i + runLength) == QLatin1Char('\\'))
            ++runLength;

        const int next = i + runLength;
        const bool escapesPunctuation =
            next < n && (text.at(next).isPunct() || text.at(next).isSymbol());

        if (escapesPunctuation && (runLength % 2) == 1)
        {
            const int marker = next - 1;
            formatData->addMaskRange(marker, marker);
            escaped.append(qMakePair(next, next));
        }

        i = next;
    }

    return escaped;
}

// ---------------------------------------------------------------------------
// Plain-parenthesis links: [title](url)
// ---------------------------------------------------------------------------
QVector<MarkdownLinkExpression> MarkdownParser::parsePlainLinks(
    const QString &text, const QVector<QPair<int, int>> &protectedRanges,
    FormatData *formatData)
{
    QVector<MarkdownLinkExpression> links;
    const int n = text.size();
    int i = 0;

    while (i < n)
    {
        if (text.at(i) != QLatin1Char('[') || isMarkdownEscaped(text, i) ||
            isProtectedPosition(i, protectedRanges))
        {
            ++i;
            continue;
        }

        // An exclamation mark turns the expression into an image, which is
        // handled by parseImages().
        if (i > 0 && text.at(i - 1) == QLatin1Char('!'))
        {
            ++i;
            continue;
        }

        int depth = 0;
        int titleEnd = -1;
        for (int cursor = i + 1; cursor < n; ++cursor)
        {
            if (isMarkdownEscaped(text, cursor))
                continue;
            const QChar c = text.at(cursor);
            if (c == QLatin1Char('['))
                ++depth;
            else if (c == QLatin1Char(']'))
            {
                if (depth == 0)
                {
                    titleEnd = cursor;
                    break;
                }
                --depth;
            }
        }

        if (titleEnd < 0 || titleEnd + 1 >= n ||
            text.at(titleEnd + 1) != QLatin1Char('('))
        {
            ++i;
            continue;
        }

        const int urlStart = titleEnd + 1;
        const int destinationStart = urlStart + 1;

        // The angle-bracket form `[title](<url>)` is handled by the marker
        // scan so that it keeps supporting spaces and parentheses.
        if (destinationStart < n &&
            text.at(destinationStart) == QLatin1Char('<'))
        {
            ++i;
            continue;
        }

        int parenDepth = 0;
        int urlEnd = -1;
        for (int cursor = destinationStart; cursor < n; ++cursor)
        {
            if (isMarkdownEscaped(text, cursor))
                continue;
            const QChar c = text.at(cursor);
            if (c == QLatin1Char('('))
                ++parenDepth;
            else if (c == QLatin1Char(')'))
            {
                if (parenDepth == 0)
                {
                    urlEnd = cursor;
                    break;
                }
                --parenDepth;
            }
        }

        if (urlEnd < 0)
        {
            ++i;
            continue;
        }

        MarkdownLinkExpression link;
        link.titleStart = i;
        link.titleEnd = titleEnd;
        link.urlStart = urlStart;
        link.urlEnd = urlEnd;
        link.title = text.mid(i + 1, titleEnd - i - 1);

        // A trailing `"title"` / `'title'` is not part of the destination.
        int bareUrlEnd = urlEnd;
        if (urlEnd > destinationStart &&
            (text.at(urlEnd - 1) == QLatin1Char('"') ||
             text.at(urlEnd - 1) == QLatin1Char('\'')))
        {
            const QChar quote = text.at(urlEnd - 1);
            int quoteOpen = -1;
            for (int cursor = urlEnd - 2; cursor > destinationStart; --cursor)
            {
                if (text.at(cursor) == quote &&
                    !isMarkdownEscaped(text, cursor))
                {
                    quoteOpen = cursor;
                    break;
                }
            }
            if (quoteOpen > destinationStart &&
                text.at(quoteOpen - 1).isSpace())
            {
                bareUrlEnd = quoteOpen - 1;
                while (bareUrlEnd > destinationStart &&
                       text.at(bareUrlEnd).isSpace())
                    --bareUrlEnd;
            }
        }

        link.url = text.mid(destinationStart,
                            bareUrlEnd - destinationStart + 1);

        formatData->addFormat(link.titleStart, link.titleEnd,
                              QStringLiteral(LINK_SYMBOL_TITLE_END), &link.url,
                              &link.title);
        formatData->addFormat(link.urlStart, link.urlEnd,
                              QStringLiteral(LINK_SYMBOL_URL_PLAIN_END),
                              nullptr, nullptr);

        links.append(link);
        i = urlEnd + 1;
    }

    return links;
}

// ---------------------------------------------------------------------------
// Autolinks: <https://example.com>
// ---------------------------------------------------------------------------
void MarkdownParser::parseAutoLinks(const QString &text, FormatData *formatData)
{
    const int n = text.size();
    int i = 0;

    while (i < n)
    {
        if (text.at(i) != QLatin1Char('<') || isMarkdownEscaped(text, i))
        {
            ++i;
            continue;
        }

        int close = -1;
        for (int cursor = i + 1; cursor < n; ++cursor)
        {
            const QChar c = text.at(cursor);
            if (c.isSpace() || c == QLatin1Char('<'))
                break;
            if (c == QLatin1Char('>') && !isMarkdownEscaped(text, cursor))
            {
                close = cursor;
                break;
            }
        }

        if (close < 0)
        {
            ++i;
            continue;
        }

        const QString destination = text.mid(i + 1, close - i - 1);
        const bool looksLikeUri =
            destination.contains(QStringLiteral("://")) ||
            destination.startsWith(QStringLiteral("mailto:"));

        if (!looksLikeUri)
        {
            ++i;
            continue;
        }

        formatData->addAutoLinkFormat(i, close, destination);
        i = close + 1;
    }
}

// ---------------------------------------------------------------------------
// Inline images
// ---------------------------------------------------------------------------
QVector<MarkdownImageExpression> MarkdownParser::parseImages(const QString &text)
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


// ---------------------------------------------------------------------------
// GFM pipe tables
// ---------------------------------------------------------------------------
namespace {

// Split a row on unescaped pipes. Outer pipes are optional.
QStringList splitRow(const QString &text)
{
    QStringList cells;
    QString current;
    const int n = text.size();

    for (int i = 0; i < n; ++i)
    {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\\') && i + 1 < n)
        {
            current.append(c);
            current.append(text.at(i + 1));
            ++i;
            continue;
        }
        if (c == QLatin1Char('|'))
        {
            cells.append(current);
            current.clear();
            continue;
        }
        current.append(c);
    }
    cells.append(current);

    // A leading pipe produces an empty first cell and a trailing pipe an empty
    // last cell; drop those.
    if (!cells.isEmpty() && cells.first().trimmed().isEmpty() &&
        text.trimmed().startsWith(QLatin1Char('|')))
        cells.removeFirst();
    if (!cells.isEmpty() && cells.last().trimmed().isEmpty() &&
        text.trimmed().endsWith(QLatin1Char('|')))
        cells.removeLast();

    return cells;
}

bool isDelimiterCell(const QString &cell)
{
    const QString trimmed = cell.trimmed();
    if (trimmed.isEmpty())
        return false;

    int i = 0;
    if (trimmed.at(i) == QLatin1Char(':'))
        ++i;
    int dashes = 0;
    while (i < trimmed.size() && trimmed.at(i) == QLatin1Char('-'))
    {
        ++dashes;
        ++i;
    }
    if (i < trimmed.size() && trimmed.at(i) == QLatin1Char(':'))
        ++i;

    return dashes >= 1 && i == trimmed.size();
}

}  // namespace

bool MarkdownParser::isTableDelimiterRow(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (!trimmed.contains(QLatin1Char('-')))
        return false;

    const QStringList cells = splitRow(trimmed);
    if (cells.isEmpty())
        return false;

    for (const QString &cell : cells)
    {
        if (!isDelimiterCell(cell))
            return false;
    }
    return true;
}

bool MarkdownParser::isTableRow(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    // A pipe must appear outside an escaped position.
    for (int i = 0; i < trimmed.size(); ++i)
    {
        if (trimmed.at(i) == QLatin1Char('|') &&
            !isMarkdownEscaped(trimmed, i))
            return true;
    }
    return false;
}

QStringList MarkdownParser::tableCells(const QString &text)
{
    QStringList cells = splitRow(text);
    for (QString &cell : cells)
        cell = cell.trimmed();
    return cells;
}

QVector<Qt::Alignment> MarkdownParser::tableAlignments(
    const QString &delimiterText)
{
    QVector<Qt::Alignment> alignments;
    const QStringList cells = tableCells(delimiterText);

    for (const QString &cell : cells)
    {
        const QString trimmed = cell.trimmed();
        const bool left = trimmed.startsWith(QLatin1Char(':'));
        const bool right = trimmed.endsWith(QLatin1Char(':'));

        if (left && right)
            alignments.append(Qt::AlignHCenter);
        else if (right)
            alignments.append(Qt::AlignRight);
        else
            alignments.append(Qt::AlignLeft);
    }

    return alignments;
}

TableInfo MarkdownParser::detectTable(const QTextDocument &document,
                                      const int blockNumber)
{
    TableInfo info;

    const QTextBlock header = document.findBlockByNumber(blockNumber);
    if (!header.isValid() || !isTableRow(header.text()))
        return info;

    const QTextBlock delimiter = header.next();
    if (!delimiter.isValid() || !isTableDelimiterRow(delimiter.text()))
        return info;

    const QStringList headerCells = tableCells(header.text());
    const QStringList delimiterCells = tableCells(delimiter.text());
    if (headerCells.isEmpty() || headerCells.size() != delimiterCells.size())
        return info;

    info.valid = true;
    info.headerBlock = header.blockNumber();
    info.delimiterBlock = delimiter.blockNumber();
    info.columnCount = headerCells.size();
    info.alignments = tableAlignments(delimiter.text());

    info.columnWidths = QVector<int>(info.columnCount, 0);
    auto trackWidths = [&info](const QStringList &cells)
    {
        const int count = qMin(cells.size(), info.columnCount);
        for (int i = 0; i < count; ++i)
            info.columnWidths[i] = qMax(info.columnWidths.at(i),
                                        cells.at(i).size());
    };
    trackWidths(headerCells);

    int last = delimiter.blockNumber();
    for (QTextBlock row = delimiter.next(); row.isValid(); row = row.next())
    {
        // The table ends at the first blank line or non-row block.
        if (!isTableRow(row.text()))
            break;
        // A delimiter row cannot appear twice in a row for the same table.
        last = row.blockNumber();
        trackWidths(tableCells(row.text()));
    }

    info.lastBlock = last;
    return info;
}

// ---------------------------------------------------------------------------
// Per-block Markdown detection
// ---------------------------------------------------------------------------
void MarkdownParser::identifyFormatData(QTextBlock &block,
                                        FormatData *existing)
{
    resetFormatLocation();
    FormatData *formatData = existing ? existing : new FormatData;
    QString text(block.text());
    QString textLink = text;
    QString test;

    formatData->setMaskSize(block.text().size());

    int index1 = 0;
    // Blockquote markers may be nested and need no trailing space, matching
    // CommonMark: up to three leading spaces, then one or more '>' markers
    // each optionally followed by a single space.
    int quoteDepth = 0;
    const int quoteContentStart = blockQuoteMarkerEnd(text, quoteDepth);
    if (quoteContentStart >= 0)
    {
        formatData->addBlockQuoteFormat(quoteContentStart - 1,
                                        text.size() - 1, quoteDepth);
    }

    insertHeadingData(text, index1, formatData);

    // Inline code spans first, so every later scanner can skip their content
    // (`` `**literal**` `` must not become bold).
    const QVector<QPair<int, int>> codeSpanRanges =
        parseInlineCodeSpans(text, formatData);

    QVector<QPair<int, int>> protectedRanges = codeSpanRanges;

    // Hide the marker backslash of escaped punctuation and protect the escaped
    // characters themselves from the scanners below.
    const QVector<QPair<int, int>> escapedRanges =
        maskEscapedCharacters(text, formatData);
    protectedRanges += escapedRanges;

    // Plain `[title](url)` links are resolved up front; the marker scan below
    // only handles the angle-bracket form `[title](<url>)`.
    const QVector<MarkdownLinkExpression> plainLinks =
        parsePlainLinks(text, protectedRanges, formatData);
    for (const auto &link : plainLinks)
    {
        // Protect the brackets and the destination only. The title text may
        // still contain emphasis or checkboxes.
        protectedRanges.append(qMakePair(link.titleStart, link.titleStart));
        protectedRanges.append(qMakePair(link.titleEnd, link.urlEnd));
    }

    // <https://example.com> - hidden angle brackets, clickable URL text.
    parseAutoLinks(text, formatData);

    // CommonMark delimiter-stack emphasis parser (replaces the old
    // three-character window heuristic for *, _, ~~).
    parseEmphasis(text, protectedRanges, formatData);

    int index2 = index1 + 1;
    int index3 = index2 + 1;

    int len = text.length();
    while (index1 < len)
    {
        // Characters owned by a code span, a plain link, or an escape are not
        // Markdown markers; jump over the whole range.
        const int protectedEnd = protectedRangeEndAt(index1, protectedRanges);
        if (protectedEnd >= index1)
        {
            index1 = protectedEnd + 1;
            index2 = index1 + 1;
            index3 = index2 + 1;
            continue;
        }

        if (isMarkdownEscaped(text, index1))
        {
            ++index1;
            index2 = index1 + 1;
            index3 = index2 + 1;
            continue;
        }

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
                    locCheck.reset();
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
    const auto imageMatches = parseImages(text);
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

    // Table rows: turn every unescaped pipe outside a code span into a tab so
    // all rows of the table line up on the shared tab stops, and hide the
    // spaces the user typed around the pipes.
    if (auto *table = dynamic_cast<TableData *>(formatData))
    {
        for (int i = 0; i < text.size(); ++i)
        {
            if (text.at(i) != QLatin1Char('|') || isMarkdownEscaped(text, i) ||
                isProtectedPosition(i, codeSpanRanges))
                continue;

            formatData->addCharacterReplacement(i, QLatin1Char('\t'));

            int left = i - 1;
            while (left >= 0 && text.at(left) == QLatin1Char(' '))
            {
                formatData->addMaskRange(left, left);
                --left;
            }
            int right = i + 1;
            while (right < text.size() && text.at(right) == QLatin1Char(' '))
            {
                formatData->addMaskRange(right, right);
                ++right;
            }
        }
        Q_UNUSED(table);
    }

    if (!formatData->isEmpty() || formatData->hasMaskedCharacters())
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
        if (existing == nullptr)
            block.setUserData(formatData);
    }
}

// ---------------------------------------------------------------------------
// ATX headings (H1-H6), including up to three leading spaces and optional
// closing hashes. Marker and closing syntax are hidden only in rendered view.
// ---------------------------------------------------------------------------
void MarkdownParser::insertHeadingData(const QString &text, int &index1,
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
MarkdownParser::parseInlineCodeSpans(const QString &text,
                                     FormatData *formatData)
{
    QVector<QPair<int, int>> ranges;
    const int n = text.length();
    int i = 0;

    while (i < n)
    {
        if (text.at(i) != QLatin1Char('`') || isMarkdownEscaped(text, i))
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
            if (text.at(j) != QLatin1Char('`') || isMarkdownEscaped(text, j))
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
void MarkdownParser::parseEmphasis(
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

void MarkdownParser::insertFormatCheckBoxData(FormatLocation &loc, int &index1,
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

void MarkdownParser::insertFormatLinkData(
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

void MarkdownParser::incrementIndexes(int &index1, int &index2, int &index3,
                                      const int size)
{
    index1 += size;
    index2 += size;
    index3 += size;
}

void MarkdownParser::convertCharacterToCheckboxSymbol(const QChar &single,
                                                      QString &text)
{
    if (single == '-' || single == ' ' || single == '[' || single == ']' ||
        single == 'x')
        text += single;
}

bool MarkdownParser::convertCharacterToLinkSymbol(const QChar &single,
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
void MarkdownParser::composeSymbolCombination(int length, const QString &text,
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

void MarkdownParser::resetFormatLocation()
{
    locCheck.reset();
    locLink.reset();
    locLinkTitle.reset();
}
