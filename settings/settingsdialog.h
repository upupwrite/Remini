#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "highlighter.h"
#include "mktextdocument.h"

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void setFont(const QFont &font);
    ~SettingsDialog();

signals:
    void syntaxColorUpdate(HighlightColor &colors);
    void updateUiSettings(const QFont &font);

    // Emitted after the user accepts the dialog ONLY when the theme really
    // changed. MainWindow listens to this and re-applies the style sheet
    // (and, on some platforms, the QStyle) without requiring a restart.
    void themeChanged(const QString &themeName);

private slots:
    void executeFolderDialog();
    void updateFontHandler(const QFont &f);
    void updateFontSizeHandler(const QString &text);
    void updateStretchHandler(const int index);
    void updateWeightHandler(const int index);
    void saveSettingsHandler();
public slots:
    void syntaxColorUpdateHandler(HighlightColor &colors);
    void show();

private:
    Ui::SettingsDialog *ui;
    HighlightColor previewColors;
    MkTextDocument previewDocument;
    Highlighter previewHighligher;

    QString vaultRootPath;
};

#endif  // SETTINGSDIALOG_H
