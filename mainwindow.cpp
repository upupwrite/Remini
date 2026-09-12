#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QString>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup_views(this, *ui);

    // "windowsvista" is Windows-only and returns nullptr on Linux/macOS.
    // Fall back to "fusion" so QApplication::setStyle() never gets nullptr.
    if (QStyleFactory::keys().contains(QStringLiteral("windowsvista"),
                                       Qt::CaseInsensitive)) {
        lightThemeStyle = QStyleFactory::create(QStringLiteral("windowsvista"));
    } else {
        lightThemeStyle = QStyleFactory::create(QStringLiteral("fusion"));
    }
    darkThemeStyle = QStyleFactory::create(QStringLiteral("fusion"));

    themeContents = darkTheme;
    themeState    = darkThemeState;
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("fusion")));
    this->setStyleSheet(themeContents);

    rightShiftTimer = new QTimer(this);
    leftShiftTimer  = new QTimer(this);

    QObject::connect(rightShiftTimer, &QTimer::timeout,
                     this, &MainWindow::shiftTimerHandle);
    QObject::connect(leftShiftTimer, &QTimer::timeout,
                     this, &MainWindow::shiftTimerHandle);

    QObject::connect(this, &MainWindow::openRecentFilesDialog,
                     view_handler.get(), &ViewsHandler::openRecentFilesDialogHandle);
    QObject::connect(this, &MainWindow::startSearchAll,
                     view_handler.get(), &ViewsHandler::startTextSearchInAllFilesHandle);
    QObject::connect(this, &MainWindow::startFileSearch,
                     view_handler.get(), &ViewsHandler::startFileSearchHandle);
    QObject::connect(this, &MainWindow::sendFocusToNavigationView,
                     view_handler.get(), &ViewsHandler::sendFocusToNavigationViewHandler);
    QObject::connect(this, &MainWindow::editLock,
                     view_handler.get(), &ViewsHandler::editLockHandle);

#ifdef Q_OS_WIN
    win = &WindowApi::instance();
    QObject::connect(win, &WindowApi::showApp,
                     this, &MainWindow::showHideApp);
#endif
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    delete rightShiftTimer;
    delete leftShiftTimer;
    delete lightThemeStyle;
    delete darkThemeStyle;
    delete ui;
}

// ---------------------------------------------------------------------------
// Key release handling
// ---------------------------------------------------------------------------
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // --- Double-Shift detection -------------------------------------------
    const int scan = event->nativeScanCode();

#ifdef Q_OS_LINUX
    // Pick the correct scan-code pair for the running session.
    // "xcb"     -> X11  (xkb keycodes)
    // "wayland" -> Wayland (evdev keycodes)
    const bool isX11 = QGuiApplication::platformName().contains(
                           QStringLiteral("xcb"), Qt::CaseInsensitive);
    const int leftShiftCode  = isX11 ? LEFT_SHIFT_KEY_X11  : LEFT_SHIFT_KEY;
    const int rightShiftCode = isX11 ? RIGHT_SHIFT_KEY_X11 : RIGHT_SHIFT_KEY;
#else
    const int leftShiftCode  = LEFT_SHIFT_KEY;
    const int rightShiftCode = RIGHT_SHIFT_KEY;
#endif

    if (scan == rightShiftCode) {
        if (rightShiftTimer->isActive())
            emit startSearchAll();
        rightShiftTimer->start(DOUBLE_SHIFT_TIMER_MS);
        leftShiftTimer->stop();
    } else if (scan == leftShiftCode) {
        if (leftShiftTimer->isActive())
            emit startFileSearch();
        leftShiftTimer->start(DOUBLE_SHIFT_TIMER_MS);
        rightShiftTimer->stop();
    } else {
        rightShiftTimer->stop();
        leftShiftTimer->stop();
    }

    // --- Other shortcuts --------------------------------------------------
    switch (event->key()) {
    case Qt::Key_Escape:
        emit sendFocusToNavigationView();
        break;

    case Qt::Key_F12:
        if (themeState == darkThemeState) {
            themeState    = lightThemeState;
            themeContents = lightTheme;
            this->setStyleSheet(themeContents);
            if (lightThemeStyle)
                QApplication::setStyle(lightThemeStyle);
        } else {
            themeState    = darkThemeState;
            themeContents = darkTheme;
            this->setStyleSheet(themeContents);
            if (darkThemeStyle)
                QApplication::setStyle(darkThemeStyle);
        }
        break;

    case Qt::Key_L:
        // Bitwise test: on X11 with NumLock, modifiers() also contains
        // Qt::KeypadModifier, so strict equality breaks Alt+L.
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
    if (this->isMinimized()) {
        this->showNormal();
    } else {
        if (QWidget::winId() == win->GetForegroundWindowInvoke()) {
            this->showMinimized();
        } else {
            this->showMinimized();
            this->showNormal();
        }
    }
#endif
    // On X11 / Wayland the WindowApi does not currently emit showApp,
    // so this slot is unreachable there. If a global hotkey is added
    // later, implement the same logic using QWindow::isActive() / raise().
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
