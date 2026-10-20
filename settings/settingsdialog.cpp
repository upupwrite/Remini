#include "settingsdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QIntValidator>
#include <QSettings>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>

#include "mktextdocument.h"
#include "theme.h"
#include "ui_settingsdialog.h"

// ---------------------------------------------------------------------------
// Default monospace font per platform.
// "Cascadia Mono" ships with Windows Terminal, so it is only a safe default
// there. Linux and macOS get a font that is essentially guaranteed to exist.
// ---------------------------------------------------------------------------
static QString defaultMonospaceFont()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Cascadia Mono");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Menlo");
#else
    return QStringLiteral("Monospace");
#endif
}

namespace
{
const QString previewText = QStringLiteral(
    "The header file declares several type definitions that guarantee a "
    "specified bit-size on all platforms supported by Qt for various basic "
    "types, for example qint8 which is a signed char guaranteed to be 8-bit on "
    "all platforms supported by Qt. The header file also declares the "
    "qlonglong type definition for long long int ( __int64 on Windows). "
    "Several convenience type definitions are declared: qreal for double or "
    "float, uchar for unsigned char, uint for unsigned int, ulong for unsigned "
    "long and ushort for unsigned short.\\n"
    "\\\"Finally, the QtMsgType definition identifies the various messages that "
    "can be generated and sent to a Qt message handler; \\\"QtMessageHandler is "
    "a type definition for a pointer to a function with the signature void "
    "myMessageHandler(QtMsgType, const QMessageLogContext &, const char *). "
    "QMessageLogContext class contains the line, file, and function the "
    "message was logged at. This information is created by the QMessageLogger "
    "class. initialize the new thread before starting its event loop, or to "
    "run parallel code without an event loop. "
    "//Comment texts examples. \\n"
    "cmake -G \\\"Visual Studio 17 2022\\\" -A x64 -S path_to_source -B "
    "\\\"build64\\\"");
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    this->setWindowModality(Qt::WindowModal);

    previewDocument.setPlainText(previewText);
    previewHighligher.setDocument(&this->previewDocument);
    ui->txt_preview->setDocument(&this->previewDocument);

    ui->cmb_mkState->addItem("Disabled");
    ui->cmb_mkState->addItem("Enabled");

    ui->cmb_lineWrap->addItem("Disabled");
    ui->cmb_lineWrap->addItem("Enabled");

    for (const Theme &t : themeAchieve::themeVec())
    {
        ui->cmb_theme->addItem(t.name);
    }

    ui->cmb_stretch->addItem("AnyStretch");
    ui->cmb_stretch->addItem("UltraCondensed");
    ui->cmb_stretch->addItem("ExtraCondensed");
    ui->cmb_stretch->addItem("Condensed");
    ui->cmb_stretch->addItem("SemiCondensed");
    ui->cmb_stretch->addItem("Unstretched");
    ui->cmb_stretch->addItem("SemiExpanded");
    ui->cmb_stretch->addItem("Expanded");
    ui->cmb_stretch->addItem("ExtraExpanded");
    ui->cmb_stretch->addItem("UltraExpanded");

    ui->cmb_weight->addItem("Thin");
    ui->cmb_weight->addItem("ExtraLight");
    ui->cmb_weight->addItem("Light");
    ui->cmb_weight->addItem("Normal");
    ui->cmb_weight->addItem("Medium");
    ui->cmb_weight->addItem("DemiBold");
    ui->cmb_weight->addItem("Bold");
    ui->cmb_weight->addItem("ExtraBold");
    ui->cmb_weight->addItem("Black");

    ui->ledit_font_size->setValidator(
        new QIntValidator(6, 30, ui->ledit_font_size));

    QObject::connect(ui->btn_vaultRootPath, &QPushButton::pressed, this,
                     &SettingsDialog::executeFolderDialog);

    QObject::connect(this, &SettingsDialog::syntaxColorUpdate,
                     &previewHighligher,
                     &Highlighter::syntaxColorUpdateHandler);

    QObject::connect(ui->cmb_font, &QFontComboBox::currentFontChanged, this,
                     &SettingsDialog::updateFontHandler);

    QObject::connect(ui->ledit_font_size, &QLineEdit::textChanged, this,
                     &SettingsDialog::updateFontSizeHandler);

    QObject::connect(ui->cmb_stretch, &QComboBox::currentIndexChanged, this,
                     &SettingsDialog::updateStretchHandler);

    QObject::connect(ui->cmb_weight, &QComboBox::currentIndexChanged, this,
                     &SettingsDialog::updateWeightHandler);

    QObject::connect(ui->btn_dialog, &QDialogButtonBox::accepted, this,
                     &SettingsDialog::saveSettingsHandler);

    QObject::connect(ui->btn_plus, &QPushButton::clicked, this,
                     [this]()
                     {
                         int size = this->ui->ledit_font_size->text().toInt();
                         size = std::min(MAXIMUM_FONT_SIZE, size + 1);
                         ui->ledit_font_size->setText(QString::number(size));
                     });

    QObject::connect(ui->btn_minus, &QPushButton::clicked, this,
                     [this]()
                     {
                         int size = this->ui->ledit_font_size->text().toInt();
                         size = std::max(MINIMUM_FONT_SIZE, size - 1);
                         ui->ledit_font_size->setText(QString::number(size));
                     });
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::setFont(const QFont &font)
{
    ui->btn_dialog->setFont(font);
    ui->cmb_mkState->setFont(font);
    ui->lbl_lineWrap->setFont(font);
    ui->cmb_lineWrap->setFont(font);
    ui->cmb_font->setFont(font);
    ui->lbl_font->setFont(font);
    ui->lbl_font_size->setFont(font);
    ui->ledit_font_size->setFont(font);
    ui->edit_vaultRootPath->setFont(font);
    ui->lbl_vaultRootPath->setFont(font);
    ui->btn_vaultRootPath->setFont(font);
    ui->lbl_markdown->setFont(font);
    ui->lbl_theme->setFont(font);
    ui->cmb_theme->setFont(font);
    ui->lbl_weight->setFont(font);
    ui->cmb_weight->setFont(font);
    ui->lbl_stretch->setFont(font);
    ui->cmb_stretch->setFont(font);
    ui->lbl_preview->setFont(font);
    ui->btn_dialog->setFont(font);
    ui->btn_dialog->button(QDialogButtonBox::Ok)->setFont(font);
    ui->btn_dialog->button(QDialogButtonBox::Cancel)->setFont(font);
}

void SettingsDialog::executeFolderDialog()
{
    // Pass `this` as parent so the dialog is properly associated with this
    // window. On Wayland this matters for centering and stacking.
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setDirectory(vaultRootPath);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);

    if (dialog.exec())
    {
        const QStringList list = dialog.selectedFiles();
        if (list.isEmpty())
            return;

        ui->edit_vaultRootPath->setText(list.first());
    }
}

void SettingsDialog::updateFontHandler(const QFont &f)
{
    QFont font = ui->txt_preview->font();
    font.setFamily(f.family());
    ui->txt_preview->setFont(font);
}

void SettingsDialog::updateFontSizeHandler(const QString &text)
{
    QFont font = ui->txt_preview->font();
    font.setPointSize(text.toInt());
    ui->txt_preview->setFont(font);
}

void SettingsDialog::updateStretchHandler(const int index)
{
    QFont font = ui->txt_preview->font();
    switch (index)
    {
    case 0:
        font.setStretch(QFont::AnyStretch);
        break;
    case 1:
        font.setStretch(QFont::UltraCondensed);
        break;
    case 2:
        font.setStretch(QFont::ExtraCondensed);
        break;
    case 3:
        font.setStretch(QFont::Condensed);
        break;
    case 4:
        font.setStretch(QFont::SemiCondensed);
        break;
    case 5:
        font.setStretch(QFont::Unstretched);
        break;
    case 6:
        font.setStretch(QFont::SemiExpanded);
        break;
    case 7:
        font.setStretch(QFont::Expanded);
        break;
    case 8:
        font.setStretch(QFont::ExtraExpanded);
        break;
    case 9:
        font.setStretch(QFont::UltraExpanded);
        break;
    default:
        break;
    }
    ui->txt_preview->setFont(font);
}

void SettingsDialog::updateWeightHandler(const int weight)
{
    QFont font = ui->txt_preview->font();

    // The combo box order must match the indices used here.
    // Current order:
    // 0: Thin, 1: ExtraLight, 2: Light, 3: Normal, 4: Medium,
    // 5: DemiBold, 6: Bold, 7: ExtraBold, 8: Black
    switch (weight)
    {
    case 0:
        font.setWeight(QFont::Thin);
        break;
    case 1:
        font.setWeight(QFont::ExtraLight);
        break;
    case 2:
        font.setWeight(QFont::Light);
        break;
    case 3:
        font.setWeight(QFont::Normal);
        break;
    case 4:
        font.setWeight(QFont::Medium);
        break;
    case 5:
        font.setWeight(QFont::DemiBold);
        break;
    case 6:
        font.setWeight(QFont::Bold);
        break;
    case 7:
        font.setWeight(QFont::ExtraBold);
        break;
    case 8:
        font.setWeight(QFont::Black);
        break;
    default:
        font.setWeight(QFont::Normal);
        break;
    }
    ui->txt_preview->setFont(font);
}

void SettingsDialog::saveSettingsHandler()
{
    QSettings settings("Remini", "Remini");
    const QFont font = ui->txt_preview->font();

    settings.setValue("font", font.family());
    settings.setValue("fontsize", font.pointSize());
    settings.setValue("stretch", font.stretch());
    settings.setValue("weight", font.weight());
    // Store markdown / linewrap as booleans to keep the type consistent with
    // the values written by MkEdit::diableMarkdown_internal().
    settings.setValue("markdown", ui->cmb_mkState->currentIndex() != 0);
    settings.setValue("linewrap", ui->cmb_lineWrap->currentIndex() != 0);
    settings.setValue("vaultPath", ui->edit_vaultRootPath->text());

    // ------------------------------------------------------------------
    // Read the previously stored theme BEFORE overwriting it. The result
    // decides whether the application needs a full style rebuild.
    //
    // We deliberately do NOT emit themeChanged on every OK click: the
    // signal triggers a QApplication::setStyle() + setStyleSheet() cycle,
    // which rebuilds every widget's style. There is no reason to pay that
    // cost when the user only changed the font size.
    // ------------------------------------------------------------------
    const QString previousTheme =
        settings.value(QStringLiteral("theme"), themes::dark().name).toString();
    const QString newTheme = ui->cmb_theme->currentText();
    settings.setValue("theme", newTheme);

    // ------------------------------------------------------------------
    // Hide the dialog before notifying the rest of the application.
    //
    // accept() hides this widget and (when the caller used exec())
    // terminates the local event loop. Doing this first guarantees the
    // dialog is off-screen when the main window rebuilds its style sheet,
    // so no re-polish can happen while the dialog is still visible.
    // ------------------------------------------------------------------
    accept();

    // ------------------------------------------------------------------
    // Apply the font change. This only touches main-window widgets, so it
    // is safe to run synchronously.
    // ------------------------------------------------------------------
    emit updateUiSettings(font);

    // ------------------------------------------------------------------
    // Only emit themeChanged when the theme actually changed. Defer the
    // emission to the next event-loop iteration so the style rebuild runs
    // after the current call stack (button click handling, accept(), the
    // updateUiSettings slot, etc.) has fully unwound.
    // ------------------------------------------------------------------
    if (newTheme != previousTheme)
    {
        QTimer::singleShot(0, this,
                           [this, newTheme]() { emit themeChanged(newTheme); });
    }
}

void SettingsDialog::syntaxColorUpdateHandler(HighlightColor &colors)
{
    this->previewColors = colors;
    emit syntaxColorUpdate(previewColors);
}

void SettingsDialog::show()
{
    // Block signals from all widgets that will be programmatically updated
    // during initialization. This prevents handler functions (e.g.
    // updateFontHandler, updateStretchHandler, etc.) from firing while we
    // are still setting up the UI.
    const QSignalBlocker blockerTheme(ui->cmb_theme);
    const QSignalBlocker blockerFont(ui->cmb_font);
    const QSignalBlocker blockerSize(ui->ledit_font_size);
    const QSignalBlocker blockerWeight(ui->cmb_weight);
    const QSignalBlocker blockerStretch(ui->cmb_stretch);
    const QSignalBlocker blockerMk(ui->cmb_mkState);
    const QSignalBlocker blockerWrap(ui->cmb_lineWrap);

    QSettings settings("Remini", "Remini");

    // ------------------------------------------------------------------
    // Retrieve stored settings with sensible defaults.
    // ------------------------------------------------------------------
    const QString fontFamily =
        settings.value("font", defaultMonospaceFont()).toString();
    const int fontSize = settings.value("fontsize", 11).toInt();
    const bool markdown = settings.value("markdown", true).toBool();
    const bool linewrap = settings.value("linewrap", true).toBool();
    const int stretch = settings.value("stretch", QFont::Unstretched).toInt();
    const int weight = settings.value("weight", QFont::Normal).toInt();
    vaultRootPath =
        settings.value("vaultPath", QDir::currentPath()).toString();

    // ------------------------------------------------------------------
    // Populate the simple combo boxes and path field.
    // ------------------------------------------------------------------
    ui->cmb_mkState->setCurrentIndex(markdown ? 1 : 0);
    ui->cmb_lineWrap->setCurrentIndex(linewrap ? 1 : 0);
    ui->edit_vaultRootPath->setText(vaultRootPath);

    // ------------------------------------------------------------------
    // Restore the saved theme selection. Without this, the combo box would
    // always show its first entry and pressing OK would silently overwrite
    // the theme. Signal blocking is already active for this widget.
    // ------------------------------------------------------------------
    const QString savedTheme =
        settings.value(QStringLiteral("theme"), themes::dark().name).toString();
    const int themeIndex = ui->cmb_theme->findText(savedTheme);
    if (themeIndex >= 0)
    {
        ui->cmb_theme->setCurrentIndex(themeIndex);
    }

    // ------------------------------------------------------------------
    // Font family: if the stored font is not present (e.g. "Cascadia Mono"
    // on Linux), findText() returns -1. Fall back to the default monospace
    // font, and if that is also missing, fall back to the first available.
    // ------------------------------------------------------------------
    int fontIndex = ui->cmb_font->findText(fontFamily);
    if (fontIndex < 0)
    {
        fontIndex = ui->cmb_font->findText(defaultMonospaceFont());
        if (fontIndex < 0 && ui->cmb_font->count() > 0)
            fontIndex = 0;
    }
    if (fontIndex >= 0)
        ui->cmb_font->setCurrentIndex(fontIndex);

    ui->ledit_font_size->setText(QString::number(fontSize));

    // ------------------------------------------------------------------
    // Map the stored QFont::Weight value to the corresponding combo index.
    //
    // Note the combo order used above:
    //   0 Thin, 1 ExtraLight, 2 Light, 3 Normal, 4 Medium,
    //   5 DemiBold, 6 Bold, 7 ExtraBold, 8 Black
    // ------------------------------------------------------------------
    int index_weight = 3; // Default to Normal.
    switch (weight)
    {
    case QFont::Thin:
        index_weight = 0;
        break;
    case QFont::ExtraLight:
        index_weight = 1;
        break;
    case QFont::Light:
        index_weight = 2;
        break;
    case QFont::Normal:
        index_weight = 3;
        break;
    case QFont::Medium:
        index_weight = 4;
        break;
    case QFont::DemiBold:
        index_weight = 5;
        break;
    case QFont::Bold:
        index_weight = 6;
        break;
    case QFont::ExtraBold:
        index_weight = 7;
        break;
    case QFont::Black:
        index_weight = 8;
        break;
    default:
        index_weight = 3;
        break;
    }
    ui->cmb_weight->setCurrentIndex(index_weight);

    // ------------------------------------------------------------------
    // Map the stored QFont::Stretch value to the corresponding combo index.
    // ------------------------------------------------------------------
    int index_stretch = 0; // Default to AnyStretch.
    switch (stretch)
    {
    case QFont::AnyStretch:
        index_stretch = 0;
        break;
    case QFont::UltraCondensed:
        index_stretch = 1;
        break;
    case QFont::ExtraCondensed:
        index_stretch = 2;
        break;
    case QFont::Condensed:
        index_stretch = 3;
        break;
    case QFont::SemiCondensed:
        index_stretch = 4;
        break;
    case QFont::Unstretched:
        index_stretch = 5;
        break;
    case QFont::SemiExpanded:
        index_stretch = 6;
        break;
    case QFont::Expanded:
        index_stretch = 7;
        break;
    case QFont::ExtraExpanded:
        index_stretch = 8;
        break;
    case QFont::UltraExpanded:
        index_stretch = 9;
        break;
    default:
        index_stretch = 6;
        break;
    }
    ui->cmb_stretch->setCurrentIndex(index_stretch);

    // ------------------------------------------------------------------
    // Map the chosen combo index back to a QFont::Weight. Doing the lookup
    // from the index (instead of static_cast'ing the raw int read from
    // QSettings) is robust against Qt5/Qt6 enum value differences and
    // guarantees we never feed setWeight() an out-of-range value.
    // ------------------------------------------------------------------
    QFont::Weight weightEnum = QFont::Normal;
    switch (index_weight)
    {
    case 0:
        weightEnum = QFont::Thin;
        break;
    case 1:
        weightEnum = QFont::ExtraLight;
        break;
    case 2:
        weightEnum = QFont::Light;
        break;
    case 3:
        weightEnum = QFont::Normal;
        break;
    case 4:
        weightEnum = QFont::Medium;
        break;
    case 5:
        weightEnum = QFont::DemiBold;
        break;
    case 6:
        weightEnum = QFont::Bold;
        break;
    case 7:
        weightEnum = QFont::ExtraBold;
        break;
    case 8:
        weightEnum = QFont::Black;
        break;
    default:
        weightEnum = QFont::Normal;
        break;
    }

    // ------------------------------------------------------------------
    // Build the preview font from the combo box's current font so that the
    // preview always reflects what the user actually sees (avoids mismatch
    // when the stored family is not available and a fallback was chosen).
    // ------------------------------------------------------------------
    QFont previewFont = ui->cmb_font->currentFont();
    previewFont.setPointSize(fontSize);
    previewFont.setWeight(weightEnum);
    previewFont.setStretch(stretch);
    ui->txt_preview->setFont(previewFont);
    ui->txt_preview->update();

    // Finally, show the dialog.
    QDialog::show();
}

const QString SettingsDialog::getVaultRootPath()
{
    const QString currentPath = QDir::currentPath();
    // QDir::filePath() handles the separator, avoiding "path"+"file"
    // concatenation bugs when CONFIG_FILE_NAME has no leading slash.
    const QString filePath = QDir(currentPath).filePath(CONFIG_FILE_NAME);
    QFile file(filePath);

    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream stream(&file);
            vaultRootPath = stream.readAll();
        }
    }
    else
    {
        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream stream(&file);
            stream << currentPath;
            vaultRootPath = currentPath;
        }
    }
    file.close();
    return vaultRootPath;
}