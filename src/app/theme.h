#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>
#include <array>

struct HighlightColor
{
    QColor type;
    QColor method;
    QColor argument;
    QColor comment;
    QColor quote;
    QColor keyword;
    QColor searchMatch;
};


// Colours used by the Markdown renderer (headings, links, quotes, code cards,
// table grid, ...). They live here so both themes control the rendered text
// instead of the renderer hard-coding values.
struct MarkdownPalette
{
    QColor surface;        // editor background
    QColor text;           // body text
    QColor muted;          // secondary text (quote text, table header)
    QColor border;         // table / card borders
    QColor accent;         // links, selection accent
    QColor codeSpan;       // inline code text
    QColor codeCard;       // fenced code card fill
    QColor codeCardBorder; // fenced code card outline
    QColor quoteText;
    QColor quoteBackground;
    QColor tableGrid;
    QColor tableHeader;
    QColor tableHeaderText;
    QColor tableRowAlternate;
    QColor horizontalRule;
    QColor hoverRow;       // hover-revealed table row
    QColor heading[6];
};

inline MarkdownPalette lightPalette()
{
    MarkdownPalette p;
    p.surface = QColor(QStringLiteral("#FBFAF8"));
    p.text = QColor(QStringLiteral("#1F2328"));
    p.muted = QColor(QStringLiteral("#6B7280"));
    p.border = QColor(QStringLiteral("#E3DED7"));
    p.accent = QColor(QStringLiteral("#2563EB"));
    p.codeSpan = QColor(QStringLiteral("#9A3412"));
    p.codeCard = QColor(QStringLiteral("#F3F1ED"));
    p.codeCardBorder = QColor(QStringLiteral("#E3DED7"));
    p.quoteText = QColor(QStringLiteral("#5B6672"));
    p.quoteBackground = QColor(37, 99, 235, 16);
    p.tableGrid = QColor(QStringLiteral("#DCD6CE"));
    p.tableHeader = QColor(QStringLiteral("#F5F2ED"));
    p.tableHeaderText = QColor(QStringLiteral("#111827"));
    p.tableRowAlternate = QColor(QStringLiteral("#FCFBF9"));
    p.horizontalRule = QColor(QStringLiteral("#DCD6CE"));
    p.hoverRow = QColor(37, 99, 235, 18);
    p.heading[0] = QColor(QStringLiteral("#12406B"));
    p.heading[1] = QColor(QStringLiteral("#1B5580"));
    p.heading[2] = QColor(QStringLiteral("#2A6A94"));
    p.heading[3] = QColor(QStringLiteral("#2A6A94"));
    p.heading[4] = QColor(QStringLiteral("#3E6B84"));
    p.heading[5] = QColor(QStringLiteral("#5B6672"));
    return p;
}

inline MarkdownPalette darkPalette()
{
    MarkdownPalette p;
    p.surface = QColor(QStringLiteral("#15171C"));
    p.text = QColor(QStringLiteral("#E7E9EC"));
    p.muted = QColor(QStringLiteral("#9BA3AE"));
    p.border = QColor(QStringLiteral("#2C313A"));
    p.accent = QColor(QStringLiteral("#6CB6FF"));
    p.codeSpan = QColor(QStringLiteral("#F0B27A"));
    p.codeCard = QColor(QStringLiteral("#1A1D24"));
    p.codeCardBorder = QColor(QStringLiteral("#2C313A"));
    p.quoteText = QColor(QStringLiteral("#9BA3AE"));
    p.quoteBackground = QColor(108, 182, 255, 20);
    p.tableGrid = QColor(QStringLiteral("#313845"));
    p.tableHeader = QColor(QStringLiteral("#21252D"));
    p.tableHeaderText = QColor(QStringLiteral("#F3F5F7"));
    p.tableRowAlternate = QColor(QStringLiteral("#1A1D23"));
    p.horizontalRule = QColor(QStringLiteral("#313845"));
    p.hoverRow = QColor(108, 182, 255, 22);
    p.heading[0] = QColor(QStringLiteral("#9CC9F5"));
    p.heading[1] = QColor(QStringLiteral("#8FBFEC"));
    p.heading[2] = QColor(QStringLiteral("#83B4E2"));
    p.heading[3] = QColor(QStringLiteral("#83B4E2"));
    p.heading[4] = QColor(QStringLiteral("#7FA6C4"));
    p.heading[5] = QColor(QStringLiteral("#9BA3AE"));
    return p;
}

enum ThemeState
{
    lightThemeState,
    darkThemeState,
};

namespace ThemeQss
{

inline const QString& lightTheme()
{
    static const QString s = QStringLiteral(
        "QMainWindow { background-color: #FBFAF8; border-style: none; }"
        "QDialog, QColorDialog, QMessageBox { background-color: #FBFAF8; }"
        "QLabel { color: #1F2328; }"
        "MkEdit {"
        "    qproperty-blockColor: #F3F1ED;"
        "    qproperty-methodColor: #6D5BD0;"
        "    qproperty-typeColor: #0F766E;"
        "    qproperty-argumentColor: #9A3412;"
        "    qproperty-commentColor: #8A9099;"
        "    qproperty-quoteColor: #2F855A;"
        "    qproperty-keywordColor: #B4236B;"
        "    qproperty-searchMatchColor: #FDE68A;"
        "    background-color: #FBFAF8;"
        "    color: #1F2328;"
        "    selection-background-color: #BBD3FB;"
        "    selection-color: #111827;"
        "    padding: 12 14 12 16;"
        "}"
        "QTreeView {"
        "    background-color: #FBFAF8;"
        "    border: none;"
        "    color: #1F2328;"
        "    padding: 4 6;"
        "}"
        "QTreeView::item { padding: 3 2; border-radius: 4px; }"
        "QTreeView::item:hover { background-color: #F1EEE9; }"
        "QTreeView::item:selected { background-color: #DCE9FB; color: #111827; }"
        "QLineEdit, QComboBox {"
        "    background-color: #FFFFFF;"
        "    color: #1F2328;"
        "    border: 1px solid #E3DED7;"
        "    border-radius: 6px;"
        "    padding: 4 8;"
        "    min-height: 20px;"
        "}"
        "QLineEdit:focus, QComboBox:focus { border-color: #2563EB; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "    background-color: #FFFFFF;"
        "    color: #1F2328;"
        "    border: 1px solid #E3DED7;"
        "    selection-background-color: #DCE9FB;"
        "    selection-color: #111827;"
        "    outline: none;"
        "}"
        "QPushButton, QToolButton {"
        "    background-color: #F1EEE9;"
        "    color: #1F2328;"
        "    border: 1px solid #E3DED7;"
        "    border-radius: 6px;"
        "    padding: 5 12;"
        "    font-weight: 600;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #E9E4DD; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #DCD6CE; }"
        "QPushButton:disabled, QToolButton:disabled { color: #9AA0A6; }"
        "QMenu {"
        "    background-color: #FFFFFF;"
        "    color: #1F2328;"
        "    border: 1px solid #E3DED7;"
        "    padding: 4px;"
        "}"
        "QMenu::item { padding: 5 18; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #DCE9FB; color: #111827; }"
        "QMenu::separator { height: 1px; background: #E3DED7; margin: 4 8; }"
        "QToolTip {"
        "    background-color: #1F2328;"
        "    color: #FBFAF8;"
        "    border: none;"
        "    padding: 4 6;"
        "}"
        "QSplitter::handle { background-color: #E3DED7; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical {"
        "    background: #CFC8BE; border-radius: 4px; min-height: 28px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #B8B0A5; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
        "QScrollBar:horizontal { background: transparent; height: 10px; border: none; }"
        "QScrollBar::handle:horizontal {"
        "    background: #CFC8BE; border-radius: 4px; min-width: 28px;"
        "}"
        "QScrollBar::handle:horizontal:hover { background: #B8B0A5; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "    height: 0px;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "    background: none;"
        "}"
        "QStatusBar { background-color: #FBFAF8; color: #6B7280; }"
        "QWidget { border-style: none; }");
    return s;
}

inline const QString& darkTheme()
{
    static const QString s = QStringLiteral(
        "QMainWindow { background-color: #15171C; border-style: none; }"
        "QDialog, QColorDialog, QMessageBox { background-color: #15171C; }"
        "QLabel { color: #E7E9EC; }"
        "MkEdit {"
        "    qproperty-blockColor: #1A1D24;"
        "    qproperty-methodColor: #C9A7FF;"
        "    qproperty-typeColor: #6FD3C7;"
        "    qproperty-argumentColor: #F0B27A;"
        "    qproperty-commentColor: #7C8591;"
        "    qproperty-quoteColor: #8AD4A0;"
        "    qproperty-keywordColor: #FF8FA3;"
        "    qproperty-searchMatchColor: #2BB340;"
        "    background-color: #15171C;"
        "    color: #E7E9EC;"
        "    selection-background-color: #2B4A72;"
        "    selection-color: #F3F5F7;"
        "    padding: 12 14 12 16;"
        "}"
        "QTreeView {"
        "    background-color: #15171C;"
        "    border: none;"
        "    color: #E7E9EC;"
        "    padding: 4 6;"
        "}"
        "QTreeView::branch { background: #15171C; }"
        "QTreeView::branch:has-siblings:!adjoins-item {"
        "    border-image: url(:/icons/line) 0;"
        "}"
        "QTreeView::branch:has-siblings:adjoins-item {"
        "    border-image: url(:/icons/line) 0;"
        "}"
        "QTreeView::branch:!has-siblings:adjoins-item {"
        "    border-image: url(:/icons/angle) 0;"
        "}"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings {"
        "    border-image: url(:/icons/arrow);"
        "}"
        "QTreeView::branch:open:has-children:!has-siblings,"
        "QTreeView::branch:open:has-children:has-siblings {"
        "    border-image: url(:/icons/downarrow);"
        "}"
        "QTreeView::item { padding: 3 2; border-radius: 4px; }"
        "QTreeView::item:hover { background-color: #1F242C; }"
        "QTreeView::item:selected { background-color: #2B4A72; color: #F3F5F7; }"
        "QLineEdit, QComboBox {"
        "    background-color: #1B1E25;"
        "    color: #E7E9EC;"
        "    border: 1px solid #2C313A;"
        "    border-radius: 6px;"
        "    padding: 4 8;"
        "    min-height: 20px;"
        "}"
        "QLineEdit:focus, QComboBox:focus { border-color: #6CB6FF; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "    background-color: #1B1E25;"
        "    color: #E7E9EC;"
        "    border: 1px solid #2C313A;"
        "    selection-background-color: #2B4A72;"
        "    selection-color: #F3F5F7;"
        "    outline: none;"
        "}"
        "QPushButton, QToolButton {"
        "    background-color: #1F242C;"
        "    color: #E7E9EC;"
        "    border: 1px solid #2C313A;"
        "    border-radius: 6px;"
        "    padding: 5 12;"
        "    font-weight: 600;"
        "}"
        "QPushButton:hover, QToolButton:hover { background-color: #262C36; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #2E353F; }"
        "QPushButton:disabled, QToolButton:disabled { color: #6B7280; }"
        "QMenu {"
        "    background-color: #1B1E25;"
        "    color: #E7E9EC;"
        "    border: 1px solid #2C313A;"
        "    padding: 4px;"
        "}"
        "QMenu::item { padding: 5 18; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #2B4A72; color: #F3F5F7; }"
        "QMenu::separator { height: 1px; background: #2C313A; margin: 4 8; }"
        "QToolTip {"
        "    background-color: #E7E9EC;"
        "    color: #15171C;"
        "    border: none;"
        "    padding: 4 6;"
        "}"
        "QSplitter::handle { background-color: #2C313A; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical {"
        "    background: #3A424E; border-radius: 4px; min-height: 28px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #4A5462; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"
        "QScrollBar:horizontal { background: transparent; height: 10px; border: none; }"
        "QScrollBar::handle:horizontal {"
        "    background: #3A424E; border-radius: 4px; min-width: 28px;"
        "}"
        "QScrollBar::handle:horizontal:hover { background: #4A5462; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "    height: 0px;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "    background: none;"
        "}"
        "QStatusBar { background-color: #15171C; color: #9BA3AE; }"
        "QWidget { border-style: none; }");
    return s;
}

}  // namespace ThemeQss

struct Theme
{
    ThemeState state;
    QString name;
    QString qss;
    MarkdownPalette palette;
};

namespace themes
{

inline const Theme& light()
{
    static const Theme t{ThemeState::lightThemeState, QStringLiteral("Light"),
                         ThemeQss::lightTheme(), lightPalette()};
    return t;
}

inline const Theme& dark()
{
    static const Theme t{ThemeState::darkThemeState, QStringLiteral("Dark"),
                         ThemeQss::darkTheme(), darkPalette()};
    return t;
}

}  // namespace themes

namespace themeAchieve
{

inline const std::array<Theme, 2>& themeVec()
{
    static const std::array<Theme, 2> arr = {themes::light(), themes::dark()};
    return arr;
}

// Return a pointer to the theme whose name matches, or nullptr if none.
// The returned pointer is stable: the Theme objects live in a function-local
// static std::array, so their address does not change over the program's life.
inline const Theme* findByName(const QString& name)
{
    for (const Theme& t : themeVec())
    {
        if (t.name == name)
            return &t;
    }
    return nullptr;
}

}  // namespace themeAchieve

#endif  // THEME_H
