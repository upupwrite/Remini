#ifndef BLOCKRENDERER_H
#define BLOCKRENDERER_H

#include "blocksupport.h"

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

// A lightweight range adapter for renderers that calculate their range while
// rendering (for example, a heading covers the whole visible block).
class BlockRenderRange final : public BlockSupport
{
public:
    BlockRenderRange(int start, int end) : start_(start), end_(end) {}

    int getStart() const override { return start_; }
    int getEnd() const override { return end_; }

private:
    int start_;
    int end_;
};

// Shared QTextDocument operations for Markdown renderers. Ranges are inclusive
// at the BlockSupport boundary and converted to QTextCursor's exclusive end
// convention in one place.
class BlockRenderer final
{
public:
    static bool selectRange(QTextCursor &cursor, const QTextBlock &block,
                            const BlockSupport &range)
    {
        if (!block.isValid())
            return false;

        const int textLength = qMax(0, block.length() - 1);
        const int start = qBound(0, range.getStart(), textLength);
        const int end = qMin(range.getEnd(), textLength - 1);
        if (end < start)
            return false;

        cursor.setPosition(block.position() + start);
        cursor.setPosition(block.position() + end + 1, QTextCursor::KeepAnchor);
        return cursor.hasSelection();
    }

    static bool applyCharacterFormat(QTextDocument *document,
                                     const QTextBlock &block,
                                     const BlockSupport &range,
                                     const QTextCharFormat &format)
    {
        if (!document)
            return false;

        QTextCursor cursor(document);
        if (!selectRange(cursor, block, range))
            return false;

        cursor.mergeCharFormat(format);
        return true;
    }
};

#endif  // BLOCKRENDERER_H
