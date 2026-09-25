#include "mainwindow.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QSettings>
#include <QString>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

#include "theme.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup_views(this, *ui);

    // ------------------------------------------------------------------
    // NOTE: We no longer cache QStyle instances.
    //
    // QApplication::setStyle(QStyle *) takes ownership of the passed
    // pointer and, when called again, deletes the previously installed
    // style before adopting the new one. Caching a pointer and re-passing
    // it therefore results in a use-after-free inside
    // QObjectPrivate::setParent_helper.
    //
    // Instead, applyThemeByName() creates a fresh QStyle on every call.
    // ------------------------------------------------------------------
    QSettings settings(QStringLiteral("Remini"), QStringLiteral("Remini"));
    const QString savedTheme =
        settings.value(QStringLiteral("theme"), themes::dark().name).toString();
    applyThemeByName(savedTheme);

    rightShiftTimer = new QTimer(this);
    leftShiftTimer = new QTimer(this);

    QObject::connect(rightShiftTimer, &QTimer::timeout, this,
                     &MainWindow::shiftTimerHandle);
    QObject::connect(leftShiftTimer, &QTimer::timeout, this,
                     &MainWindow::shiftTimerHandle);

    QObject::connect(this, &MainWindow::openRecentFilesDialog,
                     view_handler.get(),
                     &ViewsHandler::openRecentFilesDialogHandle);
    QObject::connect(this, &MainWindow::startSearchAll, view_handler.get(),
                     &ViewsHandler::startTextSearchInAllFilesHandle);
    QObject::connect(this, &MainWindow::startFileSearch, view_handler.get(),
                     &ViewsHandler::startFileSearchHandle);
    QObject::connect(this, &MainWindow::sendFocusToNavigationView,
                     view_handler.get(),
                     &ViewsHandler::sendFocusToNavigationViewHandler);
    QObject::connect(this, &MainWindow::editLock, view_handler.get(),
                     &ViewsHandler::editLockHandle);

    // ------------------------------------------------------------------
    // Live theme switching: when the user accepts the settings dialog with
    // a new theme, apply it immediately instead of requiring a restart.
    // The relay goes through ViewsHandler so that MainWindow never needs
    // to know about SettingsDialog directly.
    // ------------------------------------------------------------------
    QObject::connect(view_handler.get(), &ViewsHandler::themeChanged, this,
                     &MainWindow::applyThemeByName);

#ifdef Q_OS_WIN
    win = &WindowApi::instance();
    QObject::connect(win, &WindowApi::showApp, this, &MainWindow::showHideApp);
#endif
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    delete rightShiftTimer;
    delete leftShiftTimer;
    // No style pointers to clean up: QApplication owns the currently
    // installed style and will delete it on exit.
    delete ui;
}

// ---------------------------------------------------------------------------
// Apply a theme by name.
//
// Called from:
//   * the constructor (initial theme),
//   * the F12 handler (light/dark toggle),
//   * ViewsHandler::themeChanged when the user accepts the settings dialog.
//
// A fresh QStyle is created on every call. This is mandatory: setStyle()
// adopts the pointer, and re-passing the same pointer (which would then
// already be owned by QApplication) causes it to be deleted before the
// parent is reassigned, producing a use-after-free.
// ---------------------------------------------------------------------------
void MainWindow::applyThemeByName(const QString &name)
{
    const Theme *theme = themeAchieve::findByName(name);
    if (!theme)
        theme = &themes::dark();  // defensive fallback

    themeContents = theme->qss;
    themeState = theme->state;

    // ------------------------------------------------------------------
    // QStyle first: changing the QStyle clears widget style sheets, so
    // the order matters here.
    //
    // QStyleFactory::create() returns a brand-new heap object each time,
    // which is exactly what setStyle() expects.
    // ------------------------------------------------------------------
    QString styleName = QStringLiteral("fusion");
    if (themeState == lightThemeState)
    {
        // Prefer the native-looking style on platforms that have it.
        if (QStyleFactory::keys().contains(QStringLiteral("windowsvista"),
                                           Qt::CaseInsensitive))
        {
            styleName = QStringLiteral("windowsvista");
        }
    }

    QStyle *style = QStyleFactory::create(styleName);
    if (style)
        QApplication::setStyle(style);

    // Then the QSS. MkEdit widgets inside the window react to the new
    // qproperty-* color rules automatically; each setter emits
    // syntaxColorUpdate, which the Highlighter uses to re-run, so no
    // manual repaint or re-highlight is required here.
    this->setStyleSheet(themeContents);
}

// ---------------------------------------------------------------------------
// Key release handling
// ---------------------------------------------------------------------------
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // --- Double-Shift detection -------------------------------------------
    const int scan = event->nativeScanCode();

#ifdef Q_OS_LINUX
    const bool isX11 = QGuiApplication::platformName().contains(
        QStringLiteral("xcb"), Qt::CaseInsensitive);
    const int leftShiftCode = isX11 ? LEFT_SHIFT_KEY_X11 : LEFT_SHIFT_KEY;
    const int rightShiftCode = isX11 ? RIGHT_SHIFT_KEY_X11 : RIGHT_SHIFT_KEY;
#else
    const int leftShiftCode = LEFT_SHIFT_KEY;
    const int rightShiftCode = RIGHT_SHIFT_KEY;
#endif

    if (scan == rightShiftCode)
    {
        if (rightShiftTimer->isActive())
            emit startSearchAll();
        rightShiftTimer->start(DOUBLE_SHIFT_TIMER_MS);
        leftShiftTimer->stop();
    }
    else if (scan == leftShiftCode)
    {
        if (leftShiftTimer->isActive())
            emit startFileSearch();
        leftShiftTimer->start(DOUBLE_SHIFT_TIMER_MS);
        rightShiftTimer->stop();
    }
    else
    {
        rightShiftTimer->stop();
        leftShiftTimer->stop();
    }

    // --- Other shortcuts --------------------------------------------------
    switch (event->key())
    {
    case Qt::Key_Escape:
        emit sendFocusToNavigationView();
        break;

    case Qt::Key_F12:
        // Toggle between light and dark through the same path used by the
        // settings dialog, so all state (themeContents, themeState, QStyle)
        // stays in one place.
        if (themeState == darkThemeState)
            applyThemeByName(themes::light().name);
        else
            applyThemeByName(themes::dark().name);
        break;

    case Qt::Key_L:
        if (event->modifiers() & Qt::AltModifier)
            emit editLock();
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Recent-files dialog relay
// ---------------------------------------------------------------------------
void MainWindow::recentFilesHandler(bool show)
{
    emit openRecentFilesDialog(show);
}

// ---------------------------------------------------------------------------
// Show / hide the main window (Windows-only global hotkey callback)
// ---------------------------------------------------------------------------
void MainWindow::showHideApp()
{
#ifdef Q_OS_WIN
    if (this->isMinimized())
    {
        this->showNormal();
    }
    else
    {
        if (QWidget::winId() == win->GetForegroundWindowInvoke())
        {
            this->showMinimized();
        }
        else
        {
            this->showMinimized();
            this->showNormal();
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// Double-Shift timeout
// ---------------------------------------------------------------------------
void MainWindow::shiftTimerHandle()
{
    rightShiftTimer->stop();
    leftShiftTimer->stop();
}

// ---------------------------------------------------------------------------
// Views setup
// ---------------------------------------------------------------------------
void MainWindow::setup_views(QWidget *parent, Ui::MainWindow &ui)
{
    view_handler = ViewsHandler::getInstance(parent, ui);
}