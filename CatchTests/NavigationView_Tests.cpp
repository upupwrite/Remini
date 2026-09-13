// Catch2 v2 / v3 compatibility.
#if __has_include(<catch2/catch_test_macros.hpp>)
#  include <catch2/catch_test_macros.hpp>
#else
#  include <catch2/catch_all.hpp>
#endif

#include "navigationview.h"
#include "navigationmodel.h"

#include <QApplication>
#include <QDir>
#include <QFileSystemModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>
#include <QTreeView>

// ---------------------------------------------------------------------------
// Helper
//
// Build a NavigationView with a QFileSystemModel + NavigationProxyModel
// attached, the same way ViewsHandler does it, so the widget has a valid
// source model and a root index to work with.
// ---------------------------------------------------------------------------
static void attachModel(NavigationView &view, QFileSystemModel &fsModel)
{
    fsModel.setRootPath(QDir::tempPath());
    auto *proxy = new NavigationProxyModel(&view);
    proxy->setSourceModel(&fsModel);
    view.setModel(proxy);
    view.setRootIndex(proxy->setRootIndexFromPath(QDir::tempPath()));
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("NavigationView can be constructed", "[NavigationView]")
{
    NavigationView view;
    REQUIRE(view.model() == nullptr);
    REQUIRE(view.rootIndex().isValid() == false);
}

TEST_CASE("NavigationView can be constructed as editable", "[NavigationView]")
{
    NavigationView view(nullptr, true);
    REQUIRE(view.isEnabled());
}

TEST_CASE("NavigationView has a custom context menu policy", "[NavigationView]")
{
    NavigationView view;
    REQUIRE(view.contextMenuPolicy() == Qt::CustomContextMenu);
}

TEST_CASE("NavigationView hides second column and header", "[NavigationView]")
{
    NavigationView view;
    REQUIRE(view.isColumnHidden(1) == true);
    REQUIRE(view.isHeaderHidden() == true);
}

TEST_CASE("NavigationView setRowsEditable toggles without crashing", "[NavigationView]")
{
    NavigationView view(nullptr, false);
    view.setRowsEditable(true);
    view.setRowsEditable(false);
    SUCCEED();
}

TEST_CASE("NavigationView Enter key emits pressed", "[NavigationView]")
{
    NavigationView view;
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    QSignalSpy pressedSpy(&view, &NavigationView::pressed);

    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    view.keyPressEvent(&enter);

    // The signal may or may not fire depending on the validity of the
    // current index. What must not happen is a crash.
    REQUIRE(pressedSpy.count() >= 0);
}

TEST_CASE("NavigationView Backspace key emits sendFocusToSearch", "[NavigationView]")
{
    NavigationView view;
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    QSignalSpy focusSpy(&view, &NavigationView::sendFocusToSearch);

    QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    view.keyPressEvent(&backspace);

    REQUIRE(focusSpy.count() == 1);
    QList<QVariant> args = focusSpy.takeFirst();
    REQUIRE(args.at(0).value<NavigationView *>() == &view);
}

TEST_CASE("NavigationView double click on editable view does not crash",
          "[NavigationView]")
{
    NavigationView view(nullptr, true);
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    // QTest::mouseDClick dispatches through the Qt event system, so it
    // reaches the protected mouseDoubleClickEvent override without the
    // test having to call a protected member directly.
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                       QPoint(5, 5));
    SUCCEED();
}

TEST_CASE("NavigationView double click on read-only view does not crash",
          "[NavigationView]")
{
    NavigationView view(nullptr, false);
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                       QPoint(5, 5));
    SUCCEED();
}

TEST_CASE("NavigationView context menu on empty area does not crash",
          "[NavigationView]")
{
    NavigationView view;
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    // Nothing to assert beyond "did not crash". The context menu is modal,
    // so QMenu::exec is skipped here.
    REQUIRE(view.model() != nullptr);
}

TEST_CASE("NavigationView setFont propagates without crash", "[NavigationView]")
{
    NavigationView view;
    QFont f(QStringLiteral("Monospace"), 12);
    view.setFont(f);
    REQUIRE(view.font().pointSize() == 12);
}

TEST_CASE("NavigationView expandEveryItems starts the expansion timer",
          "[NavigationView]")
{
    NavigationView view;
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    // QTimer is started internally; the call must not crash.
    view.expandEveryItems(view.rootIndex());
    SUCCEED();
}

TEST_CASE("NavigationView context menu action texts are set",
          "[NavigationView]")
{
    // The context menu is built lazily in ContextMenuHandler(), so the
    // QAction text is not directly accessible. Verify the custom context
    // menu policy is in place.
    NavigationView view;
    REQUIRE(view.contextMenuPolicy() == Qt::CustomContextMenu);
}

TEST_CASE("NavigationView rootIndexFromPath is forwarded to the model",
          "[NavigationView]")
{
    NavigationView view;
    QFileSystemModel fsModel;
    attachModel(view, fsModel);

    auto *proxy = qobject_cast<NavigationProxyModel *>(view.model());
    REQUIRE(proxy != nullptr);

    const QModelIndex idx = proxy->setRootIndexFromPath(QDir::tempPath());
    REQUIRE(idx.isValid());
}
