#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

// Centralized hierarchical keys for QSettings.
namespace SettingsKeys
{
inline constexpr char FontFamily[] = "editor/font/family";
inline constexpr char FontSize[] = "editor/font/size";
inline constexpr char FontStretch[] = "editor/font/stretch";
inline constexpr char FontWeight[] = "editor/font/weight";
inline constexpr char MarkdownEnabled[] = "editor/markdown/enabled";
inline constexpr char LineWrapEnabled[] = "editor/lineWrap/enabled";
inline constexpr char TableHoverMode[] = "editor/tableHover/mode";
inline constexpr char WorkspaceRootPath[] = "workspace/rootPath";
inline constexpr char Theme[] = "appearance/theme";
}  // namespace SettingsKeys

#endif  // SETTINGSKEYS_H
