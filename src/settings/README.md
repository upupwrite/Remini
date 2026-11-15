# Settings module

This directory owns the Qt settings dialog and its Designer form.

## Files

- `settingsdialog.ui` — visual layout edited with Qt Designer.
- `settingsdialog.h` — dialog interface, signals, and slots.
- `settingsdialog.cpp` — widget setup, preview updates, persistence, and dialog lifecycle.
- `settingskeys.h` — centralized hierarchical `QSettings` keys used by the application.

## Settings flow

1. `SettingsDialog::show()` reads saved values from `QSettings("Remini", "Remini")` and populates the controls.
2. Font controls update the preview document so users can inspect family, size, weight, and stretch before applying them.
3. `saveSettingsHandler()` persists values and accepts the dialog.
4. `updateUiSettings` asks `ViewsHandler` to apply font changes; `themeChanged` is emitted only when the selected theme differs from the stored theme.
5. `ViewsHandler` relays theme changes to `MainWindow`, which owns application-wide style application.

## Development notes

- Keep the UI form focused on layout; put behavior in the dialog implementation.
- Keep persistent setting keys stable unless adding an explicit migration. The application uses hierarchical keys declared in `settingskeys.h`, grouped under `editor/`, `workspace/`, and `appearance/`. This refactor intentionally removes the old flat keys and the `config.ini` override; existing settings are not migrated.
- Use `QSignalBlocker` when restoring control values so initialization does not trigger preview or application handlers unexpectedly.
- Theme application is intentionally deferred after acceptance because rebuilding the application style while the dialog is visible can cause unnecessary re-polishing.
- Use `SettingsKeys` constants rather than introducing string literals for persistent keys. Keep settings access close to the feature that owns the value.
