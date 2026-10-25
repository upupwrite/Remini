# Build, test, and packaging

How to build Remini locally, how the test dependency is resolved, and how the
release packages are produced.

---

## 1. Requirements

| Requirement | Notes |
| --- | --- |
| CMake ≥ 3.16 | `FetchContent` fallback for Catch2 needs it. |
| C++17 compiler | MSVC 2022, GCC 11+, or Clang 14+. |
| Qt 6 Widgets | Also needs the `Gui` and `Test` modules. Qt 5.15 is accepted by the `find_package(QT NAMES Qt6 Qt5 …)` call but only Qt 6 is tested. |
| X11 development headers | Linux only: `libx11-dev` / `libx11`. The Alt+J global hotkey uses `XGrabKey`. |
| Git | Only for the automatic Catch2 fetch, and for cloning the repo. |
| Catch2 v3 | **Not** required manually — see §3. |

### Qt kit matching

Use a Qt installation built for the same compiler you build with. Do not pair an
MSVC Qt kit with MinGW (or vice versa); the link step fails with mismatched
runtime libraries.

---

## 2. Configure and build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The executable lands at `build/Remini` (single-config generators) or
`build/Release/Remini.exe` (Visual Studio).

Multi-config generators pick the configuration at build time:

```sh
cmake -S . -B build                                  # no CMAKE_BUILD_TYPE
cmake --build build --config Release
```

If CMake cannot find Qt:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<kit>
```

### CMake options

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | Provided by `include(CTest)`. `OFF` skips `CatchTests` entirely, which also drops the Catch2 dependency. |
| `CMAKE_BUILD_TYPE` | empty | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` (single-config generators only). |
| `CMAKE_PREFIX_PATH` | empty | Where to find Qt and (optionally) Catch2. |
| `CMAKE_DISABLE_FIND_PACKAGE_Catch2` | `OFF` | Force the bundled `FetchContent` path even if Catch2 is installed. Useful for testing the fallback. |
| `FETCHCONTENT_SOURCE_DIR_CATCH2` | empty | Point at a local Catch2 checkout to avoid any download (offline builds, mirrors). |
| `REMINI_CATCH2_TAG` | `v3.7.1` | Git tag used by the automatic Catch2 fetch (set inside `CatchTests/CMakeLists.txt`). |

### Application-only build (no tests, no Catch2)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel
```

This is what the packaging jobs use.

---

## 3. Catch2 is resolved automatically

The test sources use the Catch2 **v3** headers (`<catch2/catch_all.hpp>`,
`<catch2/catch_test_macros.hpp>`, `<catch2/catch_session.hpp>`), so the old v2
single-header (`catch.hpp`) is no longer usable.

`CatchTests/CMakeLists.txt` tries, in order:

1. `find_package(Catch2 3 QUIET)` — uses a system package, vcpkg, Conan, or
   anything on `CMAKE_PREFIX_PATH`.
2. Otherwise `FetchContent` clones the pinned tag `v3.7.1` into
   `build/_deps/catch2-src` and builds only the library.

So a fresh clone builds and tests with no manual step:

```sh
cmake -S . -B build
cmake --build build --parallel
```

To make it explicit and offline-friendly, install Catch2 v3 yourself and point
CMake at it:

```sh
# Debian / Ubuntu 24.04+
sudo apt install catch2              # provides Catch2Config.cmake

# Arch
sudo pacman -S catch2

# macOS
brew install catch2

# from source, pinned
git clone --depth 1 -b v3.7.1 https://github.com/catchorg/Catch2.git
cmake -S Catch2 -B Catch2/build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build Catch2/build --target install

cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/.local
```

Offline with a local checkout:

```sh
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_CATCH2=/path/to/Catch2
```

### Historical note

CI previously downloaded the Catch2 v2 single header and set
`CATCH_INCLUDE_DIR`. Both are gone: the project now links the `Catch2::Catch2`
target, and `CATCH_INCLUDE_DIR` is unused. If you have an old build directory,
delete it (`rm -rf build`) so the stale cache does not resurrect the old paths.

---

## 4. Running the tests

```sh
ctest --test-dir build --output-on-failure
```

or run the binary directly for filtering and richer output:

```sh
./build/CatchTests/CatchTests                     # everything
./build/CatchTests/CatchTests "[MkTextDocument]"  # parsing
./build/CatchTests/CatchTests "[MkEdit]"          # editing
./build/CatchTests/CatchTests --list-tests        # all test names
./build/CatchTests/CatchTests "MkEdit pressing*"  # by name pattern
```

### Headless environments

The test binary constructs a `QApplication`, so a display server (or the
offscreen platform plugin) is required:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

CI sets `QT_QPA_PLATFORM=offscreen` at job level.

### Windows (MSVC)

```powershell
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release
cmake --build Build --config Release
cd Build\CatchTests\Release
.\CatchTests.exe
```

`windeployqt` is needed if Qt's DLLs are not already on `PATH`:

```powershell
& "$env:QT_DIR\bin\windeployqt.exe" --no-translations .\CatchTests.exe
```

---

## 5. Continuous integration

| Workflow | Trigger | What it does |
| --- | --- | --- |
| `.github/workflows/c-cpp.yml` | push / PR to `master` | Windows (MSVC 2022, Qt 6.5.0): build, `windeployqt`, run `CatchTests.exe`. |
| `.github/workflows/ci-linux.yml` | push / PR to `main`/`master` | Ubuntu 24.04, Debug **and** Release matrix: configure with Ninja, build, `ctest`. |
| `.github/workflows/package.yml` | tag `v*` | Builds release packages for Windows, Debian, and Arch Linux and attaches them to the GitHub Release. |

### Why `windows-2022` and not `windows-latest`

`windows-latest` moved to the `windows-2025-vs2026` image (MSVC 19.51). Qt
6.5.0's `qvarlengtharray.h` still uses `stdext::checked_array_iterator`, which
newer MSVC STLs removed, so every translation unit including a Qt header fails
with `error C3861: 'stdext': identifier not found`. `windows-2022` (MSVC 19.4x)
is the toolchain Qt 6.5.0 was built against.

---

## 6. Packaging

Packaging is triggered by pushing a tag that starts with `v`:

```sh
git tag v0.1.0
git push origin v0.1.0
```

All three jobs derive the version from the tag (leading `v` stripped) and upload
their artifact both to the workflow run and to the GitHub Release.

### 6.1 Windows — `Remini-<version>-win64.zip`

1. Install Qt 6.5.0 (`win64_msvc2019_64`).
2. Configure with `-DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release`.
3. Build, copy `Remini.exe` into a staging directory.
4. Run `windeployqt --no-translations` inside the staging directory to pull in
   the Qt DLLs and platform plugins.
5. Zip the staging directory.

The archive is self-contained: unzip and run `Remini.exe`.

### 6.2 Debian — `Remini-<version>-amd64.deb`

Built inside a `debian:bookworm` container (so no host pollution and a stable
glibc baseline). Dependencies installed there: `build-essential`, `cmake`,
`git`, `ninja-build`, `pkg-config`, `qt6-base-dev`, `qt6-base-dev-tools`,
`libx11-dev`, `libxkbcommon-dev`, `libgl1-mesa-dev`.

Package layout:

```
/usr/bin/Remini
/usr/share/applications/remini.desktop
/usr/share/icons/hicolor/256x256/apps/remini.png
/usr/share/doc/remini/...        (optional)
```

The control file is `packaging/debian/control`, where `__VERSION__` is replaced
by the tag version. Runtime dependencies declared there:

```
libqt6core6, libqt6gui6, libqt6widgets6,
libx11-6, libxkbcommon0, libgl1, libegl1
```

Reproduce locally:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=OFF \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build

version=0.1.0
pkgdir=/tmp/remini_${version}_amd64
mkdir -p "$pkgdir/DEBIAN" "$pkgdir/usr/bin" \
         "$pkgdir/usr/share/applications" \
         "$pkgdir/usr/share/icons/hicolor/256x256/apps"
cp build/Remini "$pkgdir/usr/bin/Remini"
cp packaging/linux/remini.desktop "$pkgdir/usr/share/applications/"
cp resources/application.png \
   "$pkgdir/usr/share/icons/hicolor/256x256/apps/remini.png"
sed "s/__VERSION__/$version/" packaging/debian/control > "$pkgdir/DEBIAN/control"
dpkg-deb --build --root-owner-group "$pkgdir" "Remini-${version}-amd64.deb"
```

Inspect the result with `dpkg-deb -c Remini-0.1.0-amd64.deb` and
`dpkg-deb -I Remini-0.1.0-amd64.deb`.

### 6.3 Arch Linux — `remini-<version>-1-x86_64.pkg.tar.zst`

Built inside an `archlinux:latest` container with `base-devel`, `cmake`,
`ninja`, `qt6-base`, `libx11`. `makepkg` refuses to run as root, so the job
creates a `builder` user and runs makepkg as that user.

The recipe is `packaging/arch/PKGBUILD` with `pkgver=__VERSION__` substituted
from the tag. `makepkg` downloads the GitHub source tarball for the tag itself
(`source=(… tags/v$pkgver.tar.gz)`), so the local checkout is only used for the
recipe.

Reproduce locally on an Arch machine:

```sh
cd packaging/arch
sed -i 's/pkgver=__VERSION__/pkgver=0.1.0/' PKGBUILD
makepkg -f
sudo pacman -U remini-0.1.0-1-x86_64.pkg.tar.zst
```

### 6.4 Release checklist

- [ ] `git tag vX.Y.Z && git push origin vX.Y.Z`
- [ ] All three packaging jobs are green
- [ ] The Release contains the `.zip`, `.deb`, and `.pkg.tar.zst`
- [ ] `README.md` feature list matches what actually ships

---

## 7. Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `Could not find a package configuration file provided by "Catch2"` | You are on an older checkout where `find_package(Catch2 3 REQUIRED)` was unconditional, or the FetchContent download failed (no network). Update CMake files, or install Catch2 v3 and pass `-DCMAKE_PREFIX_PATH`, or use `-DFETCHCONTENT_SOURCE_DIR_CATCH2=…`. |
| `error C3861: 'stdext': identifier not found` (Windows) | Qt 6.5.0 with MSVC 19.50+. Use the `windows-2022` image or a newer Qt. |
| `qt.qpa.plugin: Could not load the Qt platform plugin "xcb"` | Headless machine: set `QT_QPA_PLATFORM=offscreen`. |
| `X11/Xlib.h: No such file or directory` | Install `libx11-dev` (Debian/Ubuntu) or `libx11` (Arch). |
| Tests fail only in CI | Check `QT_QPA_PLATFORM` is set, and that the Qt version matches (`Qt 6.5.0` on Windows). |
| Stale configuration after pulling | `rm -rf build` (or `Build`) and re-configure. |

---

## 8. Related documents

- [architecture.md](architecture.md) — source layout and function ownership.
- [markdown-parsing.md](markdown-parsing.md) — how Markdown is parsed/rendered.
- [keyboard-shortcuts.md](keyboard-shortcuts.md) — editor shortcuts.
- [cross-platform-development.md](cross-platform-development.md) — platform
  support matrix and cross-platform practices.
