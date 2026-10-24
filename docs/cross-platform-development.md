# Cross-platform development

This guide describes how to build and extend Remini across supported desktop platforms. Remini is a C++17 application built with Qt 6 Widgets and CMake.

## Platform support

| Platform | Status | Notes |
| --- | --- | --- |
| Windows | Supported by the current CI configuration | The GitHub Actions workflow builds with Qt 6.5 and MSVC 2022. |
| Linux (X11) | Buildable; platform behavior should be verified locally | The Alt+J global shortcut uses X11-specific integration and requires an X11 session and development/runtime dependencies. |
| Linux (Wayland) | Partial | The application may build, but the X11 global shortcut is skipped because Wayland does not provide the same global-grab API. |
| macOS | Experimental / not fully validated | There is no macOS CI validation or equivalent global shortcut implementation at present. |

A successful build on one platform does not guarantee that platform-specific integrations work on another. Treat CI as the minimum verified configuration and report untested platform behavior clearly.

## Prerequisites

- A C++17-capable compiler.
- CMake compatible with the version required by the project.
- Qt 6 Widgets and the Qt development tools needed by the project's CMake configuration.
- Git.
- Catch2 dependencies/configuration used by the `CatchTests` target, if running the test suite.

Use a Qt installation whose compiler kit matches your selected compiler. For example, use the MSVC Qt kit with MSVC on Windows; do not mix it with a MinGW kit.

## Configure and build

From the repository root, configure a build directory and build the application:

```sh
cmake -S . -B build
cmake --build build --config Release
```

For a multi-configuration generator such as Visual Studio, the configuration is selected at build time. For single-configuration generators such as Ninja or Makefiles, set `CMAKE_BUILD_TYPE` during configuration when a Release build is desired:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If CMake cannot find Qt, point `CMAKE_PREFIX_PATH` at the Qt installation matching your compiler, for example:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<kit>
```

Replace the example path with the actual Qt kit directory on your machine.

## Tests

Build the test target and run the tests using the generated build system. With a single-configuration generator:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

If the project exposes the CatchTests executable directly but does not register it with CTest in your configuration, run that executable from the build output directory. Check the CI workflow for the authoritative target names and current supported toolchain.

When changing Markdown parsing or rendering, add regression tests for both the intended syntax and nearby invalid syntax. Prefer deterministic tests that do not require a display server unless the behavior under test is explicitly UI/platform integration.

## Cross-platform implementation practices

### Keep platform-specific code isolated

- Put operating-system integration in `src/platform/` rather than spreading platform checks through widgets or Markdown rendering.
- Guard platform APIs with Qt's platform macros or CMake platform checks.
- Provide a safe fallback when a feature is unavailable. A shortcut that cannot be registered should not prevent the editor from starting.
- Avoid assuming that a global keyboard shortcut API available on X11 is available on Wayland, Windows, or macOS.

### Prefer Qt abstractions

Use Qt classes for paths, files, text, UI, events, and resources where practical. Avoid hard-coded path separators, assumptions about the current working directory, or platform-specific executable locations. Use Qt resource paths for bundled assets and handle missing external resources gracefully.

### Preserve module boundaries

Keep responsibilities aligned with the source layout described in [the architecture guide](architecture.md):

- `src/app/` — application startup and top-level window orchestration.
- `src/markdown/` — Markdown parsing, formatting, and rendering.
- `src/navigation/` and `src/search/` — file navigation and search.
- `src/settings/` and `src/widgets/` — settings and reusable UI.
- `src/platform/` — operating-system integration.

Do not add platform branches to the Markdown rendering path unless the rendering behavior itself genuinely differs by platform.

### Handle filesystem differences

- Use `QDir`, `QFileInfo`, and `QFile` instead of manually assembling paths.
- Do not assume filename case sensitivity is consistent across filesystems.
- Check permissions and file-operation errors instead of assuming a path is writable.
- Test Unicode paths and filenames, especially when opening and saving notes.

### Be careful with GUI tests

Qt GUI tests may need a display server on Linux. Use the project's CI configuration as a reference for required environment variables and platform setup. Do not silently skip a test that is needed to validate core behavior; make the reason for a platform-specific skip explicit.

## CI and pull-request checklist

Before submitting a cross-platform change:

1. Build with the relevant Qt kit and compiler.
2. Run the available automated tests and report any environment-related limitations.
3. Check platform-specific code paths and provide fallbacks for unsupported APIs.
4. Avoid broadening `GITHUB_TOKEN` permissions; workflows should declare the minimum permissions they need.
5. Update this guide when platform support or build prerequisites materially change.

## Known limitations

The platform matrix above reflects the repository's current validation level, not a promise that every feature is fully supported on every operating system. In particular, macOS and Wayland behavior should be treated as unverified or partial until dedicated CI and platform-specific implementations are added.
