// Catch2 v2 / v3 compatibility.
#if __has_include(<catch2/catch_test_macros.hpp>)
#  include <catch2/catch_test_macros.hpp>
#else
#  include <catch2/catch_all.hpp>
#endif

#include "recentfilesdialog.h"

#include <QApplication>
#include <QListWidget>
#include <QScopedPointer>
#include <QTest>

// ---------------------------------------------------------------------------
// Helper: build a RecentFilesDialog that owns its own list widget.
//
// RecentFilesDialog's constructor calls listWidget->setParent(this), so the
// dialog takes ownership. The test must NOT delete the list separately, or
// it would be destroyed twice when the dialog goes out of scope.
// ---------------------------------------------------------------------------
static QScopedPointer<RecentFilesDialog> makeDialog(QListWidget *&listOut)
{
    auto *list = new QListWidget;
    listOut = list;
    return QScopedPointer<RecentFilesDialog>(new RecentFilesDialog(nullptr, list));
}

TEST_CASE("selection when list is empty", "[RecentFilesDialog]")
{
    const int expectedRow = -1;

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    dialog->show();

    REQUIRE(listPtr->currentRow() == expectedRow);
    // No manual delete: dialog owns listPtr.
}

TEST_CASE("selection in 2nd item test", "[RecentFilesDialog]")
{
    const int expectedRow = 1;

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    listPtr->addItem("text1.txt");
    listPtr->addItem("test2.txt");
    dialog->show();

    REQUIRE(listPtr->currentRow() == expectedRow);
}

TEST_CASE("selection in 2nd item and correct path test", "[RecentFilesDialog]")
{
    const QString expectedPath("path.txt");

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    listPtr->addItem("text1.txt");
    listPtr->addItem(expectedPath);
    dialog->show();

    REQUIRE(dialog->getCurrentRelativeFile() == expectedPath);
}

TEST_CASE("selection in 1st item and correct path test", "[RecentFilesDialog]")
{
    const QString expectedPath("path.txt");

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    listPtr->addItem(expectedPath);
    listPtr->addItem("text2.txt");
    dialog->show();

    // show() selects index 1 because count > 1; Tab wraps to index 0.
    QTest::keyPress(dialog.data(), Qt::Key_Tab);

    REQUIRE(dialog->getCurrentRelativeFile() == expectedPath);
}

TEST_CASE("adding paths to the list", "[RecentFilesDialog]")
{
    const QString expectedPath("path.txt");

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    dialog->updateRecentFileHandle(expectedPath);   // list = [expectedPath]
    listPtr->addItem("text2.txt");                  // list = [expectedPath, text2]
    dialog->show();                                 // count > 1 -> selects index 1

    QTest::keyPress(dialog.data(), Qt::Key_Tab);    // wraps to index 0

    REQUIRE(dialog->getCurrentRelativeFile() == expectedPath);
}

TEST_CASE("adding path to the dialog using slots", "[RecentFilesDialog]")
{
    const QString item1 = "test1.txt";

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    dialog->updateRecentFileHandle(item1);
    dialog->show();

    REQUIRE(dialog->getCurrentRelativeFile() == item1);
}

TEST_CASE("adding 2 paths to the dialog using slots with show()", "[RecentFilesDialog]")
{
    const QString item1 = "test1.txt";

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    dialog->updateRecentFileHandle(item1);
    dialog->show();

    REQUIRE(dialog->getCurrentRelativeFile() == item1);
}

TEST_CASE("adding 2 paths to the dialog using slots without show()", "[RecentFilesDialog]")
{
    const QString item1 = "test1.txt";
    const QString item2 = "test2.txt";

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    // updateRecentFileHandle inserts at index 0 and selects it,
    // so the most recent path is always item2.
    dialog->updateRecentFileHandle(item1);
    dialog->updateRecentFileHandle(item2);

    REQUIRE(dialog->getCurrentRelativeFile() == item2);
}

TEST_CASE("adding more paths to the dialog using slots", "[RecentFilesDialog]")
{
    const QString item1 = "test1.txt";
    const QString item2 = "test2.txt";
    const QString item3 = "test3.txt";

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    dialog->updateRecentFileHandle(item1);
    dialog->updateRecentFileHandle(item2);
    dialog->updateRecentFileHandle(item3);
    // List order after 3 inserts at index 0: [item3, item2, item1]

    dialog->show();       // count > 1 -> selects index 1 (item2)
    QTest::keyPress(dialog.data(), Qt::Key_Tab);   // moves to index 2 (item1)

    REQUIRE(dialog->getCurrentRelativeFile() == item1);
}

TEST_CASE("cycling back to the last file", "[RecentFilesDialog]")
{
    const QString item1 = "test1.txt";
    const QString item2 = "test2.txt";
    const QString item3 = "test3.txt";

    QListWidget *listPtr = nullptr;
    auto dialog = makeDialog(listPtr);

    dialog->updateRecentFileHandle(item1);
    dialog->updateRecentFileHandle(item2);
    dialog->updateRecentFileHandle(item3);
    // List order: [item3, item2, item1]

    dialog->show();       // selects index 1 (item2)
    QTest::keyPress(dialog.data(), Qt::Key_Tab);   // index 2 (item1)
    QTest::keyPress(dialog.data(), Qt::Key_Tab);   // wraps to index 0 (item3)

    REQUIRE(dialog->getCurrentRelativeFile() == item3);
}
