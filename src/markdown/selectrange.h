#ifndef SELECTRANGE_H
#define SELECTRANGE_H

#include <set>

#define NO_SELECTION_POS -1

// Cursor/selection snapshot shared by value between the editor view and the
// Markdown document while symbols are hidden or revealed. It deliberately
// carries no pointer back to either side, so both can consume it safely.
struct SelectRange
{
    bool hasSelection = false;
    bool isCheckBox = false;
    int arrowBlock = NO_SELECTION_POS;
    int arrowPosInBlock = NO_SELECTION_POS;
    int currentposInBlock = NO_SELECTION_POS;
    int currentBlockNo = NO_SELECTION_POS;
    int selectionFirstStartBlock = NO_SELECTION_POS;
    int selectionFirstStartPosInBlock = NO_SELECTION_POS;
    int selectionEndBlock = NO_SELECTION_POS;
    int selectionEndPosInBlock = NO_SELECTION_POS;
    bool isFirstMousePress = false;
    int scrollValue;
    std::set<int> hideBlocks;
    std::set<int> showBlocks;
};

struct RawBlockInfo
{
    bool hasSelection = false;
    int rawFirstBlock = NO_SELECTION_POS;
    int rawEndBlock = NO_SELECTION_POS;
};

#endif  // SELECTRANGE_H
