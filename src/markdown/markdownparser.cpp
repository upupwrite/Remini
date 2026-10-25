#include "markdownparser.h"

#include "formatdata.h"

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

// A delimiter run is "protected" if it sits entirely inside one of the ranges
// returned by parseInlineCodeSpans(). Because code spans are delimited by
// backticks, a delimiter run is either fully inside a range or fully outside -
// there is no partial overlap.
bool isProtectedPosition(int pos, const QVector<QPair<int, int>> &ranges)
{
    for (const auto &r : ranges)
    {
        if (pos >= r.first && pos <= r.second)
            return true;
    }
    return false;
}

bool isMarkdownEscaped(const QString &text, int position)
{
    int backslashes = 0;
    for (int i = position - 1; i >= 0 && text.at(i) == QLatin1Char('\\'); --i)
        ++backslashes;
    return (backslashes % 2) != 0;
}

}  // namespace

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
// Per-block Markdown detection
// ---------------------------------------------------------------------------
void MarkdownParser::identifyFormatData(QTextBlock &block)
{
    resetFormatLocation();
    FormatData *formatData = new FormatData;
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
