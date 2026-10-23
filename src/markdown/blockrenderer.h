#ifndef BLOCKRENDERER_H
#define BLOCKRENDERER_H

#include "blocksupport.h"

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

// Inclusive character offsets are used by Markdown fragments. QTextCursor
// uses an exclusive end position, so all conversion and bounds checks live here.
class BlockRenderRange final : public BlockSupport
{
public:
    BlockRenderRange(int start, int end) noexcept : start_(start), end_(end) {}

    int getStart() const override { return start_; }
    int getEnd() const override { return end_; }

private:
    int start_;
    int end_;
};

class BlockRenderer final
{
public:
    // Select a non-empty range inside the visible text of `block`. The
    // paragraph separator is intentionally excluded. Returns false for an
    // invalid block, a different document, or an empty/out-of-bounds range.
    static bool selectRange(QTextCursor &cursor, const QTextBlock &block,
                            const BlockSupport &range)
    {
        if (!block.isValid() || cursor.document() != block.document())
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

    // Apply a character format to a valid, non-empty inclusive range.
    // Returns false instead of modifying the document when inputs are invalid.
    static bool applyCharacterFormat(QTextDocument *document,
                                     const QTextBlock &block,
                                     const BlockSupport &range,
                                     const QTextCharFormat &format)
    {
        if (!document || block.document() != document)
            return false;

        QTextCursor cursor(document);
        if (!selectRange(cursor, block, range))
            return false;

        cursor.mergeCharFormat(format);
        return true;
    }
};

#endif  // BLOCKRENDERER_H
