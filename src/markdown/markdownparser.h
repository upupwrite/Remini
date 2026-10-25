#ifndef MARKDOWNPARSER_H
#define MARKDOWNPARSER_H

#include <QChar>
#include <QPair>
#include <QString>
#include <QTextBlock>
#include <QVector>

class FormatData;

// Result of scanning one inline Markdown image expression.
struct MarkdownImageExpression
{
    int start = -1;
    int end = -1;  // exclusive
    QString source;
    QString url;
    QString alt;
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
    void identifyFormatData(QTextBlock &block);

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
