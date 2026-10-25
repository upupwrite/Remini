#include <catch2/catch_test_macros.hpp>

#include "editcommand.h"
#include "edittypes.h"
#include "mktextdocument.h"
#include "selectrange.h"

// These tests exercise EditCommand through MkTextDocument alone, without an
// MkEdit widget, to prove the undo/redo commands no longer reach back into the
// editor through raw pointers.

TEST_CASE("EditCommand single-edit undo/redo works without an editor view",
          "[EditCommand]")
{
    MkTextDocument doc;
    doc.setPlainText("**bold**");

    // Apply a single-block edit as the editor would, then record its snapshot.
    doc.setUndoRedoText(0, "**bolder**");

    EditSnapshot snapshot;
    snapshot.editType = singleEdit;
    snapshot.oldBlock = "**bold**";
    snapshot.blockNo = 0;
    snapshot.posInBlock = 6;
    snapshot.scrollValue = 0;
    snapshot.selectRange.currentBlockNo = 0;
    snapshot.selectRange.currentposInBlock = 6;

    doc.undoStackPush(snapshot);

    EditRestore restore;
    doc.undoStackUndo(restore);
    REQUIRE(restore.success);
    REQUIRE(restore.editType == singleEdit);
    REQUIRE(doc.toPlainText() == "**bold**");

    doc.undoStackRedo(restore);
    REQUIRE(restore.success);
    REQUIRE(restore.editType == singleEdit);
    REQUIRE(doc.toPlainText() == "**bolder**");
    REQUIRE(restore.selectRange.currentBlockNo == 0);
    REQUIRE(restore.selectRange.currentposInBlock == 6);
}

TEST_CASE("EditCommand multi-edit undo/redo restores raw text", "[EditCommand]")
{
    MkTextDocument doc;
    doc.setPlainText("line1\nline2");

    doc.setUndoRedoText("line1\nline2 changed");

    EditSnapshot snapshot;
    snapshot.editType = multiEdit;
    snapshot.oldText = "line1\nline2";
    snapshot.blockNo = 1;
    snapshot.posInBlock = 5;
    snapshot.scrollValue = 0;
    snapshot.selectRange.currentBlockNo = 1;
    snapshot.selectRange.currentposInBlock = 5;

    doc.undoStackPush(snapshot);

    EditRestore restore;
    doc.undoStackUndo(restore);
    REQUIRE(restore.success);
    REQUIRE(restore.editType == multiEdit);
    REQUIRE(doc.toPlainText() == "line1\nline2");

    doc.undoStackRedo(restore);
    REQUIRE(restore.success);
    REQUIRE(restore.editType == multiEdit);
    REQUIRE(doc.toPlainText() == "line1\nline2 changed");
}

TEST_CASE("EditCommand reports failure when the stack is empty", "[EditCommand]")
{
    MkTextDocument doc;
    doc.setPlainText("text");

    EditRestore restore;
    doc.undoStackUndo(restore);
    REQUIRE_FALSE(restore.success);

    doc.undoStackRedo(restore);
    REQUIRE_FALSE(restore.success);
}
