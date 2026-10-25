#ifndef BLOCKSTRUCTURE_H
#define BLOCKSTRUCTURE_H

#include <QTextBlock>
#include <QTextDocument>
#include <QVector>
#include <Qt>

// A multi-block Markdown construct (a fenced code block or a pipe table).
//
// The document used to classify these constructs inline, with one bespoke scan
// per construct. They are now described by this single value type so that
//
//   * one pass finds every multi-block construct,
//   * `MkTextDocument::identifyUserData` only maps a role to the matching
//     user-data object, and
//   * inline parsing stays exactly the same for all of them.
struct BlockStructure
{
    enum class Kind
    {
        CodeFence,
        Table,
    };

    enum class Role
    {
        Start,      // first fence line
        Content,    // fence body
        End,        // closing fence line
        Header,     // table header row
        Delimiter,  // table column/alignment row
        Body,       // table body row
    };

    Kind kind = Kind::CodeFence;
    int firstBlock = -1;
    int lastBlock = -1;

    // Table layout; unused for code fences.
    int columnCount = 0;
    QVector<int> columnWidths;
    QVector<Qt::Alignment> alignments;

    bool isValid() const { return firstBlock >= 0 && lastBlock >= firstBlock; }
    bool contains(int blockNo) const
    {
        return blockNo >= firstBlock && blockNo <= lastBlock;
    }

    Role roleAt(int blockNo) const
    {
        if (kind == Kind::CodeFence)
        {
            if (blockNo <= firstBlock)
                return Role::Start;
            if (blockNo >= lastBlock)
                return Role::End;
            return Role::Content;
        }

        if (blockNo <= firstBlock)
            return Role::Header;
        if (blockNo == firstBlock + 1)
            return Role::Delimiter;
        return Role::Body;
    }
};

// Finds every multi-block construct in a document in a single pass.
//
// Precedence is "first match wins" per block: a fenced code block swallows the
// lines inside it, so a table-like line inside a fence is not a table.
class BlockStructureScanner
{
public:
    explicit BlockStructureScanner(const QTextDocument &document);

    // Sorted by `firstBlock`.
    QVector<BlockStructure> scan() const;

private:
    const QTextDocument &document;
};

// True when the block currently shows rendered Markdown (markers hidden, table
// pipes replaced by tabs, images collapsed to an object character). Such a block
// must never be used as a source of truth, and its decorations must not be
// drawn. Shared by the document, the renderer, and the editor's painter.
bool isBlockRendered(const QTextBlock &block);

#endif  // BLOCKSTRUCTURE_H
