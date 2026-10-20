# Settings module

This directory owns the Qt settings dialog and its Designer form.

## Files

- `settingsdialog.ui` — visual layout edited with Qt Designer.
- `settingsdialog.h` — dialog interface, signals, and slots.
- `settingsdialog.cpp` — widget setup, preview updates, persistence, and dialog lifecycle.

## Settings flow

1. `SettingsDialog::show()` reads saved values from `QSettings("Remini", "Remini")` and populates the controls.
2. Font controls update the preview document so users can inspect family, size, weight, and stretch before applying them.
3. `saveSettingsHandler()` persists values and accepts the dialog.
4. `updateUiSettings` asks `ViewsHandler` to apply font changes; `themeChanged` is emitted only when the selected theme differs from the stored theme.
5. `ViewsHandler` relays theme changes to `MainWindow`, which owns application-wide style application.

## Development notes

- Keep the UI form focused on layout; put behavior in the dialog implementation.
- Keep persistent setting keys stable unless adding an explicit migration. The dialog uses `font`, `fontsize`, `stretch`, `weight`, `markdown`, `linewrap`, `vaultPath`, and `theme`. `ViewsHandler` also has legacy vault-path handling through the `vault` key and optional `config.ini`; treat this as existing compatibility behavior rather than silently renaming keys.
- Use `QSignalBlocker` when restoring control values so initialization does not trigger preview or application handlers unexpectedly.
- Theme application is intentionally deferred after acceptance because rebuilding the application style while the dialog is visible can cause unnecessary re-polishing.
- The settings dialog currently shares a few settings concerns with `ViewsHandler`; a follow-up extraction of a dedicated settings repository would further reduce duplicated `QSettings` access.
