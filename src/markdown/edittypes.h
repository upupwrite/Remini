#ifndef EDITTYPES_H
#define EDITTYPES_H

// Classification of an edit used by the undo/redo stack. The value describes
// how the pending change should be recorded and later restored.
enum EditType
{
    undoRedo = 0,
    singleEdit,
    multiDelete,
    checkbox,
    enterPressed,
    multiEdit,
};

#endif  // EDITTYPES_H
