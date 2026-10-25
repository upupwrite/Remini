#include "tabledata.h"

TableData::TableData(RowKind kind)
    : FormatData(),
      rowKind(kind),
      columnCount(0),
      tableStartBlock(-1),
      tableEndBlock(-1)
{
}

TableData::RowKind TableData::getRowKind() const { return rowKind; }

void TableData::setRowKind(RowKind kind) { rowKind = kind; }

void TableData::setColumnCount(int count) { columnCount = count; }

int TableData::getColumnCount() const { return columnCount; }

void TableData::setAlignments(const QVector<Qt::Alignment> &alignments)
{
    this->alignments = alignments;
}

Qt::Alignment TableData::getAlignment(int column) const
{
    if (column < 0 || column >= alignments.size())
        return Qt::AlignLeft;
    return alignments.at(column);
}

void TableData::setTabStops(const QVector<qreal> &stops) { tabStops = stops; }

const QVector<qreal> &TableData::getTabStops() const { return tabStops; }

void TableData::setColumnWidths(const QVector<int> &widths)
{
    columnWidths = widths;
}

const QVector<int> &TableData::getColumnWidths() const { return columnWidths; }

TableData *TableData::cloneLayout() const
{
    auto *copy = new TableData(rowKind);
    copy->setColumnCount(columnCount);
    copy->setAlignments(alignments);
    copy->setColumnWidths(columnWidths);
    copy->setTabStops(tabStops);
    copy->setTableRange(tableStartBlock, tableEndBlock);
    return copy;
}

void TableData::setTableRange(int startBlock, int endBlock)
{
    tableStartBlock = startBlock;
    tableEndBlock = endBlock;
}

int TableData::getTableStartBlock() const { return tableStartBlock; }

int TableData::getTableEndBlock() const { return tableEndBlock; }
