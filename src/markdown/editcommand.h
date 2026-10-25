#ifndef EDITCOMMAND_H
#define EDITCOMMAND_H

#include <QString>
#include <QUndoCommand>

#include "edittypes.h"
#include "selectrange.h"

class MkTextDocument;

// Snapshot of an edit before it is committed to the undo stack. It carries
// plain values only; there are no pointers back into the editor view.
struct EditSnapshot
{
    EditType editType = multiEdit;
    QString oldText;   // full raw-document text (multi-block edits)
    QString oldBlock;  // single raw block text (singleEdit)
    int blockNo = -1;
    int posInBlock = -1;
    int scrollValue = 0;
    SelectRange selectRange;
};

// Result of an undo/redo operation, returned by value so the editor never has
// to expose mutable members to the command object.
struct EditRestore
{
    bool success = false;
    EditType editType = multiEdit;
    SelectRange selectRange;
};

// A single undoable edit. It depends only on the Markdown document, not on the
// editor widget that produced it.
class EditCommand : public QUndoCommand
{
public:
    EditCommand(MkTextDocument *document, const EditSnapshot &snapshot);

    void undo() override;
    void redo() override;

    EditRestore restore() const { return restore_; }

private:
    MkTextDocument *document_;
    EditType editType_;
    QString undoText_;
    QString redoText_;
    int blockNo_;
    int posInBlock_;
    SelectRange undoSelectRange_;
    EditRestore restore_;
    bool isConstructorRedo_;
};

#endif  // EDITCOMMAND_H
