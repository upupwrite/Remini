#include "blockstructure.h"

#include "blockdata.h"
#include "formatdata.h"
#include "linedata.h"
#include "markdownparser.h"

#include <QtGlobal>

namespace {

bool detectTable(const QTextDocument &document, int blockNumber,
                 BlockStructure &structure)
{
    const QTextBlock header = document.findBlockByNumber(blockNumber);
    if (!header.isValid() || !MarkdownParser::isTableRow(header.text()))
        return false;

    const QTextBlock delimiter = header.next();
    if (!delimiter.isValid() ||
        !MarkdownParser::isTableDelimiterRow(delimiter.text()))
        return false;

    const QStringList headerCells = MarkdownParser::tableCells(header.text());
    const QStringList delimiterCells =
        MarkdownParser::tableCells(delimiter.text());
    if (headerCells.isEmpty() || headerCells.size() != delimiterCells.size())
        return false;

    structure.kind = BlockStructure::Kind::Table;
    structure.firstBlock = header.blockNumber();
    structure.columnCount = headerCells.size();
    structure.alignments = MarkdownParser::tableAlignments(delimiter.text());
    structure.columnWidths = QVector<int>(structure.columnCount, 0);

    auto trackWidths = [&structure](const QStringList &cells) {
        const int count = qMin(cells.size(), structure.columnCount);
        for (int i = 0; i < count; ++i)
            structure.columnWidths[i] =
                qMax(structure.columnWidths.at(i), cells.at(i).size());
    };
    trackWidths(headerCells);

    int lastBlock = delimiter.blockNumber();
    for (QTextBlock row = delimiter.next(); row.isValid(); row = row.next())
    {
        if (!MarkdownParser::isTableRow(row.text()))
            break;
        lastBlock = row.blockNumber();
        trackWidths(MarkdownParser::tableCells(row.text()));
    }

    structure.lastBlock = lastBlock;
    return true;
}

}  // namespace

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

        BlockStructure structure;
        if (!detectTable(document, blockNo, structure))
            continue;

        structures.append(structure);
        blockNo = structure.lastBlock;
    }

    // An unterminated fence is not a structure: it must stay visible while the
    // user is still typing it (the editor inserts the closing fence eagerly in
    // autoCompleteCodeBlock, so a lone fence is genuinely incomplete input).
    return structures;
}

bool isBlockRendered(const QTextBlock &block)
{
    QTextBlockUserData *data = block.userData();
    if (!data)
        return false;

    if (auto *blockData = dynamic_cast<BlockData *>(data))
        return blockData->isHidden();
    if (auto *lineData = dynamic_cast<LineData *>(data))
        return lineData->isHidden();
    if (auto *formatData = dynamic_cast<FormatData *>(data))
        return formatData->isHidden();

    return false;
}
