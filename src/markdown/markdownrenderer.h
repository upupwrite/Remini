#ifndef MARKDOWNRENDERER_H
#define MARKDOWNRENDERER_H

#include <QPair>
#include <QString>
#include <QTextBlock>
#include <QVector>
#include <tuple>

#include "blockdata.h"
#include "formatcollection.h"
#include "formatdata.h"

class QTextDocument;

// Stateless Markdown rendering primitives. These helpers know how to draw or
// hide syntax for a single block given its parsed user-data, but they do not
// track cursor/selection state or emit editor signals. The document keeps the
// "when" logic; the renderer keeps the "how".
class MarkdownRenderer final
{
public:
    static void applyMkFormat(QTextDocument *document, QTextBlock &block,
                              const BlockSupport &span,
                              FragmentData::FormatSymbol status,
                              FormatCollection &formatCollection);

    static void applyCheckBoxLinkEffect(
        FormatData *data, QTextBlock &block,
        QVector<QPair<int, int>> &checkMarkPositions,
        QVector<std::tuple<int, int, int, const QString *>> &linkPositions);

    static void hideSymbols(QTextBlock &block, const QString &symbol);
    static void showSymbols(QTextBlock &block, const QString &symbol);
    static void hideSymbolsAtPos(QString &text, int pos, const QString &symbol);
    static void showSymbolsAtPos(QString &text, int pos, const QString &symbol);

    static void removeCheckBoxLinkMousePosition(
        QTextBlock &block,
        QVector<QPair<int, int>> &checkMarkPositions,
        QVector<std::tuple<int, int, int, const QString *>> &linkPositions);

    static void setCodeBlockMargin(QTextBlock &block, int leftMargin = 0,
                                   int rightMargin = 0, int topMargin = 0,
                                   int bottomMargin = 0);
    static void resetTextBlockFormat(QTextBlock block);

    static void showHideCodeBlock(QTextDocument *document, BlockData *data,
                                  bool hide, int fontSize);
};

#endif  // MARKDOWNRENDERER_H
