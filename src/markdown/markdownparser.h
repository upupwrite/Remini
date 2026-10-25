#ifndef MARKDOWNPARSER_H
#define MARKDOWNPARSER_H

#include <QChar>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>
#include <QVector>
#include <Qt>

class FormatData;
class TableData;

// Description of one GFM pipe table found in a document.
struct TableInfo
{
    bool valid = false;
    int headerBlock = -1;
    int delimiterBlock = -1;
    int lastBlock = -1;
    int columnCount = 0;
    QVector<int> columnWidths;
    QVector<Qt::Alignment> alignments;
};



// Result of scanning one inline Markdown image expression.
struct MarkdownImageExpression
{
    int start = -1;
    int end = -1;  // exclusive
    QString source;
    QString url;
    QString alt;
};

// Result of scanning one `[title](url)` link expression.
struct MarkdownLinkExpression
{
    int titleStart = -1;
    int titleEnd = -1;  // inclusive, index of ']'
    int urlStart = -1;  // inclusive, index of '('
    int urlEnd = -1;    // inclusive, index of ')'
    QString url;
    QString title;
};

// Stateless-at-call-time Markdown parser. It turns block text into
// FormatData fragments (and, for images, their metadata) without mutating the
// rendered QTextDocument. The tiny per-pass location state lives inside the
// parser instance and is reset on every identifyFormatData() call.
class MarkdownParser
{
public:
    // Scan `text` for inline image expressions (![alt](url) / ![alt](<url>)).
    static QVector<MarkdownImageExpression> parseImages(const QString &text);

    // Returns the exact blockquote marker prefix (e.g. "> ", "> > ") when
    // `text` starts a blockquote, otherwise an empty string. Used by the
    // editor to continue a quote on Enter.
    static QString blockQuotePrefix(const QString &text);

    // Parse one block and attach a FormatData user-data object when the block
    // actually contains Markdown. Empty results leave userData untouched.
    //
    // When `existing` is supplied the caller keeps ownership and the parse is
    // written into it; this is how a table row reuses its TableData.
    void identifyFormatData(QTextBlock &block, FormatData *existing = nullptr);

    // --- Tables ------------------------------------------------------------
    // A GFM pipe table is a header row, a delimiter row, and zero or more body
    // rows. Detection works on plain text so the document can classify blocks
    // before it parses them.
    static bool isTableDelimiterRow(const QString &text);
    static bool isTableRow(const QString &text);

    // Cell text of a row, split on unescaped '|'. Outer pipes are optional and
    // surrounding spaces are trimmed from every cell.
    static QStringList tableCells(const QString &text);

    // Column alignments declared by a delimiter row.
    static QVector<Qt::Alignment> tableAlignments(const QString &delimiterText);

    // Inspect the document starting at `blockNumber`. Returns a valid TableInfo
    // when a header/delimiter pair starts there.
    static TableInfo detectTable(const QTextDocument &document,
                                 const int blockNumber);

private:
    struct FormatLocation
    {
        int start = -1;
        int end = -1;

        void reset()
        {
            start = -1;
            end = -1;
        }
    };

    FormatLocation locCheck;
    FormatLocation locLink;
    FormatLocation locLinkTitle;

    void resetFormatLocation();
    // Returns the index just past the leading blockquote marker(s), or -1 if
    // the line does not start a valid blockquote marker. Also reports the
    // nesting depth (number of '>' markers).
    static int blockQuoteMarkerEnd(const QString &text, int &depth);

    // Plain-parenthesis links `[title](url)`. The angle-bracket form
    // `[title](<url>)` is handled by the marker scan in
    // identifyFormatData(); these are collected separately and protected from
    // the marker scan.
    QVector<MarkdownLinkExpression>
    parsePlainLinks(const QString &text,
                    const QVector<QPair<int, int>> &protectedRanges,
                    FormatData *formatData);

    // `<https://example.com>` autolinks.
    void parseAutoLinks(const QString &text, FormatData *formatData);

    // Hide the backslash of a backslash-escaped punctuation character so that
    // `\*` renders as `*` instead of starting emphasis. Returns the positions
    // of the escaped characters, which must be protected from the emphasis and
    // marker scanners.
    QVector<QPair<int, int>> maskEscapedCharacters(const QString &text,
                                                   FormatData *formatData);

    static bool isProtectedPosition(int pos,
                                    const QVector<QPair<int, int>> &ranges);
    static int protectedRangeEndAt(int pos,
                                   const QVector<QPair<int, int>> &ranges);

    void insertHeadingData(const QString &text, int &index1,
                           FormatData *formatData);
    QVector<QPair<int, int>> parseInlineCodeSpans(const QString &text,
                                                  FormatData *formatData);
    void parseEmphasis(const QString &text,
                       const QVector<QPair<int, int>> &protectedRanges,
                       FormatData *formatData);
    void insertFormatCheckBoxData(FormatLocation &loc, int &index1, int &index2,
                                  int &index3, FormatData *formatData,
                                  const QString &test);
    void insertFormatLinkData(FormatLocation &locTitle, FormatLocation &locLink,
                              int &index1, int &index2, int &index3,
                              FormatData *formatData, const QString &test,
                              const QString *linkUrl, const QString *linkTitle);
    void incrementIndexes(int &index1, int &index2, int &index3,
                          const int size = 1);
    void convertCharacterToCheckboxSymbol(const QChar &single, QString &text);
    bool convertCharacterToLinkSymbol(const QChar &single, QString &text);
    void composeSymbolCombination(int length, const QString &text, int &index1,
                                  int &index2, int &index3, QString &result);
};

#endif  // MARKDOWNPARSER_H
