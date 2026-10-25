#ifndef TABLEDATA_H
#define TABLEDATA_H

#include <QTextBlockUserData>
#include <QVector>
#include <Qt>

#include "formatdata.h"

// Per-block user data for one row of a GFM pipe table.
//
// It derives from FormatData so every existing code path (inline formatting,
// masking, hidden fragments, hide/show) keeps working unchanged, and
// table-specific consumers can narrow it with dynamic_cast.
//
// A table is a run of blocks:
//
//   | name | qty |     <- Header
//   | :--- | --: |     <- Delimiter (collapsed to a separator line)
//   | tea  | 2   |     <- Body
//
// Every row shares the same column widths, tab stops, and block range so the
// rows line up even though they are separate QTextBlocks.
class TableData : public FormatData
{
public:
    enum RowKind
    {
        Header,
        Delimiter,
        Body,
    };

    explicit TableData(RowKind kind = Body);
    ~TableData() override = default;

    RowKind getRowKind() const;
    void setRowKind(RowKind kind);

    void setColumnCount(int count);
    int getColumnCount() const;

    // Alignment parsed from the delimiter row (`:---`, `---:`, `:---:`).
    void setAlignments(const QVector<Qt::Alignment> &alignments);
    Qt::Alignment getAlignment(int column) const;

    // Tab stops shared by every row, in pixels from the block's left margin.
    void setTabStops(const QVector<qreal> &stops);
    const QVector<qreal> &getTabStops() const;

    // Widest cell text of each column, in characters. Used to build the stops.
    void setColumnWidths(const QVector<int> &widths);
    const QVector<int> &getColumnWidths() const;

    // A fresh TableData with the same table layout but no parse results. Used
    // when a row is re-parsed after an edit.
    TableData *cloneLayout() const;

    void setTableRange(int startBlock, int endBlock);
    int getTableStartBlock() const;
    int getTableEndBlock() const;

private:
    RowKind rowKind;
    int columnCount;
    QVector<Qt::Alignment> alignments;
    QVector<qreal> tabStops;
    QVector<int> columnWidths;
    int tableStartBlock;
    int tableEndBlock;
};

#endif  // TABLEDATA_H
