#include "markdownconnections.h"

#include "mkedit.h"
#include "mktextdocument.h"

#include <QObject>

void connectMarkdownEditorToDocument(MkEdit &editor, MkTextDocument &document)
{
    QObject::connect(&editor, &MkEdit::checkRightClockOnCodeBlock, &document,
                     &MkTextDocument::checkRightClockOnCodeBlockHandle);
    QObject::connect(&editor, &MkEdit::selectBlockCopy, &document,
                     &MkTextDocument::selectBlockCopyHandle);
    QObject::connect(&editor, &MkEdit::duplicateLine, &document,
                     &MkTextDocument::duplicateLineHandle);
    QObject::connect(&editor, &MkEdit::smartSelection, &document,
                     &MkTextDocument::smartSelectionHandle);
    QObject::connect(&editor, &MkEdit::cursorPosChanged, &document,
                     &MkTextDocument::cursorPosChangedHandle);
    QObject::connect(&editor, &MkEdit::enterKeyPressed, &document,
                     &MkTextDocument::enterKeyPressedHandle);
    QObject::connect(&editor, &MkEdit::quoteLeftKeyPressed, &document,
                     &MkTextDocument::quoteLeftKeyPressedHandle);
    QObject::connect(&editor, &MkEdit::removeAllMkData, &document,
                     &MkTextDocument::removeAllMkDataHandle);
    QObject::connect(&editor, &MkEdit::applyAllMkData, &document,
                     &MkTextDocument::applyAllMkDataHandle);
    QObject::connect(&editor, &MkEdit::applyMkSingleBlock, &document,
                     &MkTextDocument::applyMkSingleBlockHandle);
    QObject::connect(&editor, &MkEdit::saveSingleRawBlock, &document,
                     &MkTextDocument::saveSingleRawBlockHandler);
    QObject::connect(&editor, &MkEdit::saveEnterPressedRawBlock, &document,
                     &MkTextDocument::saveEnterPressRawBlockHandler);
    QObject::connect(&editor, &MkEdit::saveRawDocument, &document,
                     &MkTextDocument::saveRawDocumentHandler);
    QObject::connect(&editor, &MkEdit::setMarkdownStatus, &document,
                     &MkTextDocument::setMarkdownHandle);
    QObject::connect(&editor, &MkEdit::pushCheckBox, &document,
                     &MkTextDocument::pushCheckBoxHandle);
    QObject::connect(&editor, &MkEdit::pushLink, &document,
                     &MkTextDocument::pushLinkHandle);
    QObject::connect(&editor, &MkEdit::autoInsertSymbol, &document,
                     &MkTextDocument::autoInsertSymbolHandle);
    QObject::connect(&editor, &MkEdit::cursorUpdate, &document,
                     &MkTextDocument::cursorUpdateHandle);
    QObject::connect(&editor, &MkEdit::undoStackPushSignal, &document,
                     &MkTextDocument::undoStackPush);
    QObject::connect(&editor, &MkEdit::undoStackUndoSignal, &document,
                     &MkTextDocument::undoStackUndo);
    QObject::connect(&editor, &MkEdit::undoStackRedoSignal, &document,
                     &MkTextDocument::undoStackRedo);

    QObject::connect(&document, &MkTextDocument::connectCurosPos, &editor,
                     &MkEdit::connectSignals);
    QObject::connect(&document, &MkTextDocument::disconnectCursorPos, &editor,
                     &MkEdit::disconnectSignals);
}

void disconnectMarkdownEditorFromDocument(MkEdit &editor,
                                          MkTextDocument &document)
{
    QObject::disconnect(&editor, &MkEdit::checkRightClockOnCodeBlock, &document,
                        &MkTextDocument::checkRightClockOnCodeBlockHandle);
    QObject::disconnect(&editor, &MkEdit::selectBlockCopy, &document,
                        &MkTextDocument::selectBlockCopyHandle);
    QObject::disconnect(&editor, &MkEdit::duplicateLine, &document,
                        &MkTextDocument::duplicateLineHandle);
    QObject::disconnect(&editor, &MkEdit::smartSelection, &document,
                        &MkTextDocument::smartSelectionHandle);
    QObject::disconnect(&editor, &MkEdit::cursorPosChanged, &document,
                        &MkTextDocument::cursorPosChangedHandle);
    QObject::disconnect(&editor, &MkEdit::enterKeyPressed, &document,
                        &MkTextDocument::enterKeyPressedHandle);
    QObject::disconnect(&editor, &MkEdit::quoteLeftKeyPressed, &document,
                        &MkTextDocument::quoteLeftKeyPressedHandle);
    QObject::disconnect(&editor, &MkEdit::removeAllMkData, &document,
                        &MkTextDocument::removeAllMkDataHandle);
    QObject::disconnect(&editor, &MkEdit::applyAllMkData, &document,
                        &MkTextDocument::applyAllMkDataHandle);
    QObject::disconnect(&editor, &MkEdit::applyMkSingleBlock, &document,
                        &MkTextDocument::applyMkSingleBlockHandle);
    QObject::disconnect(&editor, &MkEdit::saveSingleRawBlock, &document,
                        &MkTextDocument::saveSingleRawBlockHandler);
    QObject::disconnect(&editor, &MkEdit::saveEnterPressedRawBlock, &document,
                        &MkTextDocument::saveEnterPressRawBlockHandler);
    QObject::disconnect(&editor, &MkEdit::saveRawDocument, &document,
                        &MkTextDocument::saveRawDocumentHandler);
    QObject::disconnect(&editor, &MkEdit::setMarkdownStatus, &document,
                        &MkTextDocument::setMarkdownHandle);
    QObject::disconnect(&editor, &MkEdit::pushCheckBox, &document,
                        &MkTextDocument::pushCheckBoxHandle);
    QObject::disconnect(&editor, &MkEdit::pushLink, &document,
                        &MkTextDocument::pushLinkHandle);
    QObject::disconnect(&editor, &MkEdit::autoInsertSymbol, &document,
                        &MkTextDocument::autoInsertSymbolHandle);
    QObject::disconnect(&editor, &MkEdit::cursorUpdate, &document,
                        &MkTextDocument::cursorUpdateHandle);
    QObject::disconnect(&editor, &MkEdit::undoStackPushSignal, &document,
                        &MkTextDocument::undoStackPush);
    QObject::disconnect(&editor, &MkEdit::undoStackUndoSignal, &document,
                        &MkTextDocument::undoStackUndo);
    QObject::disconnect(&editor, &MkEdit::undoStackRedoSignal, &document,
                        &MkTextDocument::undoStackRedo);

    QObject::disconnect(&document, &MkTextDocument::connectCurosPos, &editor,
                        &MkEdit::connectSignals);
    QObject::disconnect(&document, &MkTextDocument::disconnectCursorPos,
                        &editor, &MkEdit::disconnectSignals);
}
