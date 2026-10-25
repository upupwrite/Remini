#include "editcommand.h"

#include "mktextdocument.h"

#include <QChar>
#include <QTextBlock>

EditCommand::EditCommand(MkTextDocument *document,
                         const EditSnapshot &snapshot)
    : document_(document),
      editType_(snapshot.editType),
      blockNo_(snapshot.blockNo),
      posInBlock_(snapshot.posInBlock),
      isConstructorRedo_(true)
{
    undoSelectRange_ = snapshot.selectRange;
    undoSelectRange_.scrollValue = snapshot.scrollValue;

    switch (editType_)
    {
    case undoRedo:
        return;

    case singleEdit:
        undoText_ = snapshot.oldBlock;
        {
            const QTextBlock viewBlock =
                document_->findBlockByNumber(snapshot.blockNo);
            redoText_ = viewBlock.isValid() ? viewBlock.text() : QString();

            // A rendered image is represented by U+FFFC in QTextBlock::text().
            // In that case, use the raw Markdown snapshot so redo preserves
            // the original ![alt](path) expression. For ordinary text edits,
            // the view snapshot is necessary because some callers/tests may
            // not have wired saveSingleRawBlock yet.
            if (redoText_.contains(QChar::ObjectReplacementCharacter))
            {
                QTextBlock sourceBlock =
                    document_->getRawDocument()->findBlockByNumber(
                        snapshot.blockNo);
                if (sourceBlock.isValid())
                    redoText_ = sourceBlock.text();
            }
        }
        undoSelectRange_.isCheckBox = false;
        break;

    case multiDelete:
    case checkbox:
    case enterPressed:
    case multiEdit:
        undoText_ = snapshot.oldText;
        redoText_ = document_ ? document_->getRawDocument()->toPlainText()
                              : QString();
        break;
    }
}

void EditCommand::undo()
{
    switch (editType_)
    {
    case undoRedo:
        return;

    case singleEdit:
        document_->setUndoRedoText(undoSelectRange_.currentBlockNo, undoText_);
        break;

    case multiDelete:
    case checkbox:
    case enterPressed:
    case multiEdit:
        document_->setUndoRedoText(undoText_);
        break;
    }

    restore_.editType = editType_;
    restore_.selectRange = undoSelectRange_;
}

void EditCommand::redo()
{
    // QUndoStack::push() calls redo() on the freshly pushed command even
    // though the edit was already applied, so skip that first invocation.
    if (isConstructorRedo_)
    {
        isConstructorRedo_ = false;
        return;
    }

    switch (editType_)
    {
    case undoRedo:
        return;

    case singleEdit:
        document_->setUndoRedoText(blockNo_, redoText_);
        break;

    case multiDelete:
    case checkbox:
    case enterPressed:
    case multiEdit:
        document_->setUndoRedoText(redoText_);
        break;
    }

    restore_.editType = editType_;
    restore_.selectRange.hasSelection = false;
    restore_.selectRange.currentBlockNo = blockNo_;
    restore_.selectRange.currentposInBlock = posInBlock_;
    restore_.selectRange.isCheckBox = undoSelectRange_.isCheckBox;
    restore_.selectRange.scrollValue = undoSelectRange_.scrollValue;
}
