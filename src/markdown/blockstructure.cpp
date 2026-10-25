#include "blockstructure.h"

#include "markdownparser.h"

BlockStructureScanner::BlockStructureScanner(const QTextDocument &document)
    : document(document)
{
}

QVector<BlockStructure> BlockStructureScanner::scan() const
{
    QVector<BlockStructure> structures;

    const int lastBlockNo = document.blockCount() - 1;
    bool insideFence = false;
    BlockStructure fence;

    for (int blockNo = 0; blockNo <= lastBlockNo; ++blockNo)
    {
        const QTextBlock block = document.findBlockByNumber(blockNo);
        if (!block.isValid())
            continue;

        const QString text = block.text();

        if (MarkdownParser::isFenceLine(text))
        {
            if (!insideFence)
            {
                insideFence = true;
                fence = BlockStructure{};
                fence.kind = BlockStructure::Kind::CodeFence;
                fence.firstBlock = blockNo;
            }
            else
            {
                insideFence = false;
                fence.lastBlock = blockNo;
                structures.append(fence);
            }
            continue;
        }

        if (insideFence)
            continue;

        const TableInfo table = MarkdownParser::detectTable(document, blockNo);
        if (!table.valid)
            continue;

        BlockStructure structure;
        structure.kind = BlockStructure::Kind::Table;
        structure.firstBlock = table.headerBlock;
        structure.lastBlock = table.lastBlock;
        structure.columnCount = table.columnCount;
        structure.columnWidths = table.columnWidths;
        structure.alignments = table.alignments;
        structures.append(structure);

        blockNo = table.lastBlock;
    }

    // An unterminated fence is not a structure: it must stay visible while the
    // user is still typing it (the editor inserts the closing fence eagerly in
    // autoCompleteCodeBlock, so a lone fence is genuinely incomplete input).
    return structures;
}
