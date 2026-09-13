#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStringList>
#include <QTextEdit>
#include <QTextStream>
#include <QToolButton>
#include <algorithm>
#include "mktextdocument.h"
#include "theme.h"

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

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    this->setWindowModality(Qt::WindowModal);

    normal   = ui->stackedWidget->widget(0);
    edit     = ui->stackedWidget->widget(1);
    file     = ui->stackedWidget->widget(2);
    markdown = ui->stackedWidget->widget(3);

    txt_preview        = findChild<QTextEdit*>("txt_preview");
    cmb_theme          = findChild<QComboBox*>("cmb_theme");
    cmb_stretch        = findChild<QComboBox*>("cmb_stretch");
    cmb_weight         = findChild<QComboBox*>("cmb_weight");
    cmb_font           = findChild<QFontComboBox*>("cmb_font");
    ledit_font_size    = findChild<QLineEdit*>("ledit_font_size");
    btn_plus           = findChild<QToolButton*>("btn_plus");
    btn_minus          = findChild<QToolButton*>("btn_minus");
    cmb_mkState        = findChild<QComboBox*>("cmb_mkState");
    cmb_lineWrap       = findChild<QComboBox*>("cmb_lineWrap");
    edit_vaultRootPath = findChild<QLineEdit*>("edit_vaultRootPath");
    btn_vaultRootPath  = findChild<QPushButton*>("btn_vaultRootPath");
    btn_dialog         = findChild<QDialogButtonBox*>("btn_dialog");

    Q_ASSERT(txt_preview && cmb_theme && cmb_stretch && cmb_weight && cmb_font
             && ledit_font_size && btn_plus && btn_minus && cmb_mkState
             && cmb_lineWrap && edit_vaultRootPath && btn_vaultRootPath && btn_dialog);

    previewDocument.setPlainText(previewText);
    previewHighligher.setDocument(&this->previewDocument);
    txt_preview->setDocument(&this->previewDocument);

    cmb_mkState->addItem("Disabled");
    cmb_mkState->addItem("Enabled");

    cmb_lineWrap->addItem("Disabled");
    cmb_lineWrap->addItem("Enabled");

    for (const Theme &t : themeAchieve::themeVec()) {
        cmb_theme->addItem(t.name);
    }

    cmb_stretch->addItem("AnyStretch");
    cmb_stretch->addItem("UltraCondensed");
    cmb_stretch->addItem("ExtraCondensed");
    cmb_stretch->addItem("Condensed");
    cmb_stretch->addItem("SemiCondensed");
    cmb_stretch->addItem("Unstretched");
    cmb_stretch->addItem("SemiExpanded");
    cmb_stretch->addItem("Expanded");
    cmb_stretch->addItem("ExtraExpanded");
    cmb_stretch->addItem("UltraExpanded");

    cmb_weight->addItem("Thin");        // 0
    cmb_weight->addItem("ExtraLight");  // 1
    cmb_weight->addItem("Light");       // 2
    cmb_weight->addItem("Normal");      // 3
    cmb_weight->addItem("Medium");      // 4
    cmb_weight->addItem("DemiBold");    // 5
    cmb_weight->addItem("Bold");        // 6
    cmb_weight->addItem("ExtraBold");   // 7
    cmb_weight->addItem("Black");       // 8

    ledit_font_size->setValidator(new QIntValidator(6, 30, ledit_font_size));

    connect(btn_vaultRootPath, &QPushButton::pressed,
            this, &SettingsDialog::executeFolderDialog);

    connect(this, &SettingsDialog::syntaxColorUpdate,
            &previewHighligher, &Highlighter::syntaxColorUpdateHandler);

    connect(cmb_font, &QFontComboBox::currentFontChanged,
            this, &SettingsDialog::updateFontHandler);

    connect(ledit_font_size, &QLineEdit::textChanged,
            this, &SettingsDialog::updateFontSizeHandler);

    connect(cmb_stretch, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateStretchHandler);

    connect(cmb_weight, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateWeightHandler);

    connect(btn_dialog, &QDialogButtonBox::accepted,
            this, &SettingsDialog::saveSettingsHandler);
    connect(btn_dialog, &QDialogButtonBox::rejected,
            this, &SettingsDialog::close);

    connect(btn_plus, &QToolButton::clicked, this, [this]() {
        int size = ledit_font_size->text().toInt();
        size = std::min(MAXIMUM_FONT_SIZE, size + 1);
        ledit_font_size->setText(QString::number(size));
    });

    connect(btn_minus, &QToolButton::clicked, this, [this]() {
        int size = ledit_font_size->text().toInt();
        size = std::max(MINIMUM_FONT_SIZE, size - 1);
        ledit_font_size->setText(QString::number(size));
    });
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setFont(const QFont &font)
{
    const auto allWidgets = findChildren<QWidget*>();
    for (QWidget *w : allWidgets) {
        w->setFont(font);
    }
    if (btn_dialog) {
        if (auto *ok = btn_dialog->button(QDialogButtonBox::Ok))
            ok->setFont(font);
        if (auto *cancel = btn_dialog->button(QDialogButtonBox::Cancel))
            cancel->setFont(font);
    }
}

void SettingsDialog::executeFolderDialog()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setDirectory(vaultRootPath);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);

    if (dialog.exec()) {
        const QStringList list = dialog.selectedFiles();
        if (list.isEmpty())
            return;
        edit_vaultRootPath->setText(list.first());
    }
}

void SettingsDialog::updateFontHandler(const QFont &f)
{
    QFont font = txt_preview->font();
    font.setFamily(f.family());
    txt_preview->setFont(font);
}

void SettingsDialog::updateFontSizeHandler(const QString &text)
{
    QFont font = txt_preview->font();
    font.setPointSize(text.toInt());
    txt_preview->setFont(font);
}

void SettingsDialog::updateStretchHandler(const int index)
{
    QFont font = txt_preview->font();
    switch (index) {
    case 0: font.setStretch(QFont::AnyStretch);      break;
    case 1: font.setStretch(QFont::UltraCondensed);  break;
    case 2: font.setStretch(QFont::ExtraCondensed);  break;
    case 3: font.setStretch(QFont::Condensed);       break;
    case 4: font.setStretch(QFont::SemiCondensed);   break;
    case 5: font.setStretch(QFont::Unstretched);     break;
    case 6: font.setStretch(QFont::SemiExpanded);    break;
    case 7: font.setStretch(QFont::Expanded);        break;
    case 8: font.setStretch(QFont::ExtraExpanded);   break;
    case 9: font.setStretch(QFont::UltraExpanded);   break;
    default: break;
    }
    txt_preview->setFont(font);
}

void SettingsDialog::updateWeightHandler(const int index)
{
    QFont font = txt_preview->font();
    switch (index) {
    case 0: font.setWeight(QFont::Thin);       break;
    case 1: font.setWeight(QFont::ExtraLight); break;
    case 2: font.setWeight(QFont::Light);      break;
    case 3: font.setWeight(QFont::Normal);     break;
    case 4: font.setWeight(QFont::Medium);     break;
    case 5: font.setWeight(QFont::DemiBold);   break;
    case 6: font.setWeight(QFont::Bold);       break;
    case 7: font.setWeight(QFont::ExtraBold);  break;
    case 8: font.setWeight(QFont::Black);      break;
    default: break;
    }
    txt_preview->setFont(font);
}

void SettingsDialog::applySettingsHandler()
{
    QSettings settings("Remini", "Remini");
    const QFont font = txt_preview->font();

    settings.setValue("font", font.family());
    settings.setValue("fontsize", font.pointSize());
    settings.setValue("stretch", font.stretch());
    settings.setValue("weight", font.weight());
    settings.setValue("markdown", cmb_mkState->currentIndex() != 0);
    settings.setValue("linewrap", cmb_lineWrap->currentIndex() != 0);
    settings.setValue("vaultPath", edit_vaultRootPath->text());
    settings.setValue("theme", cmb_theme->currentText());
    emit updateUiSettings(font);
}

void SettingsDialog::saveSettingsHandler()
{
    applySettingsHandler();
    this->close();
}

void SettingsDialog::syntaxColorUpdateHandler(HighlightColor &colors)
{
    this->previewColors = colors;
    emit syntaxColorUpdate(previewColors);
}

void SettingsDialog::show()
{
    QSettings settings("Remini", "Remini");

    const QString fontFamily = settings.value("font", defaultMonospaceFont()).toString();
    const int fontSize = settings.value("fontsize", 11).toInt();
    const bool markdown = settings.value("markdown", true).toBool();
    const bool linewrap = settings.value("linewrap", true).toBool();
    const int stretch = settings.value("stretch", QFont::Unstretched).toInt();
    const int weight = settings.value("weight", QFont::Normal).toInt();
    vaultRootPath = settings.value("vaultPath", QDir::currentPath()).toString();

    cmb_mkState->setCurrentIndex(markdown ? 1 : 0);
    cmb_lineWrap->setCurrentIndex(linewrap ? 1 : 0);
    edit_vaultRootPath->setText(vaultRootPath);

    int fontIndex = cmb_font->findText(fontFamily);
    if (fontIndex < 0) {
        fontIndex = cmb_font->findText(defaultMonospaceFont());
        if (fontIndex < 0 && cmb_font->count() > 0)
            fontIndex = 0;
    }
    if (fontIndex >= 0)
        cmb_font->setCurrentIndex(fontIndex);

    ledit_font_size->setText(QString::number(fontSize));

    int index_weight;
    switch (weight) {
    case QFont::Thin:       index_weight = 0; break;
    case QFont::ExtraLight: index_weight = 1; break;
    case QFont::Light:      index_weight = 2; break;
    case QFont::Normal:     index_weight = 3; break;
    case QFont::Medium:     index_weight = 4; break;
    case QFont::DemiBold:   index_weight = 5; break;
    case QFont::Bold:       index_weight = 6; break;
    case QFont::ExtraBold:  index_weight = 7; break;
    case QFont::Black:      index_weight = 8; break;
    default:                index_weight = 3;
    }
    cmb_weight->setCurrentIndex(index_weight);

    int index_stretch = 5;
    switch (stretch) {
    case QFont::AnyStretch:     index_stretch = 0; break;
    case QFont::UltraCondensed: index_stretch = 1; break;
    case QFont::ExtraCondensed: index_stretch = 2; break;
    case QFont::Condensed:      index_stretch = 3; break;
    case QFont::SemiCondensed:  index_stretch = 4; break;
    case QFont::Unstretched:    index_stretch = 5; break;
    case QFont::SemiExpanded:   index_stretch = 6; break;
    case QFont::Expanded:       index_stretch = 7; break;
    case QFont::ExtraExpanded:  index_stretch = 8; break;
    case QFont::UltraExpanded:  index_stretch = 9; break;
    default:                    index_stretch = 5;
    }
    cmb_stretch->setCurrentIndex(index_stretch);

    QFont font(fontFamily, fontSize, weight);
    font.setStretch(stretch);
    txt_preview->setFont(font);
    txt_preview->update();

    QDialog::show();
}

const QString SettingsDialog::getVaultRootPath()
{
    const QString currentPath = QDir::currentPath();
    const QString filePath = QDir(currentPath).filePath(CONFIG_FILE_NAME);
    QFile file(filePath);

    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            vaultRootPath = stream.readAll();
        }
    } else {
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream stream(&file);
            stream << currentPath;
            vaultRootPath = currentPath;
        }
    }
    file.close();
    return vaultRootPath;
}
