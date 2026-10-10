# InkFeed analysis (PocketBook InkPad One, Qt Quick app)

Source analysed (read-only): `<InkFeed checkout>` (git describe at analysis time: `v1.0.0-2-g901306f`).
Nothing was built or executed. Everything below is derived from reading files, plus a few read-only `ls`/`file`/`readelf`/`unzip -l` checks. Statements that could not be verified are marked **(unverified)**.

## 1. Overview

InkFeed is an RSS/Atom reader for the 2026 PocketBook InkPad One (PB1030, RK3566). It is a native Qt 6 / Qt Quick application, MIT licensed. It builds for the device (ARM cross-compile) and for desktop Linux (development and backend tests). Upstream: `github.com/jrohde/InkFeed` (README, `docs/install.md`).

## 2. Language and project structure

Languages: C++17 (`CMAKE_CXX_STANDARD 17`, `CMakeLists.txt:49`), C (bundled SQLite amalgamation `lib/sqlite3.c`), QML (`qml/`), POSIX shell (launcher and packaging script).

```
CMakeLists.txt            single build definition (251 lines)
qml.qrc                   qrc for qml/Main.qml, qml/InkButton.qml
qml/Main.qml (1180 l.)    entire UI: one ApplicationWindow with a StackView
qml/InkButton.qml (31 l.) flat e-ink button
res/inkfeed.qrc           embeds desktop_app_inkfeed.bmp + i18n/{nl,es,fr,de}.json
res/*.bmp,png,svg         drawer icons (bmp), logos
src/main_qml.cpp          entry point (108 l.)
src/qml_bridge.cpp (866)  C++ <-> QML controller (QmlBridge, context property "bridge")
src/qml_models.cpp (410)  QAbstractListModels (collections, feeds, cards, article body blocks)
src/core/                 app, feed_manager (+OPML), sync_engine, rss_parser (libxml2), readability, html_utils, i18n
src/storage/database.cpp  SQLite wrapper
src/utils/http.cpp        libcurl HTTP client
src/utils/image_cache.cpp QNetworkAccessManager-based image cache
include/*.h               headers (qml_bridge.h, qml_models.h, i18n.h are moc'd)
lib/sqlite3.{c,h}         vendored SQLite
platform/pocketbook/inkfeed.app   shell launcher (NOT the binary)
scripts/package-inkpad-one.sh     builds the install zip
docs/install.md, docs/inkpad-one-dev-notes.md
tests/test_runner.cpp     backend tests (Linux only, no Qt)
.github/workflows/{linux,inkpad-one,release}.yml
dist/                     a previously built zip (inkfeed-inkpad-one-1.0.0-2-g901306f.zip)
build_arm/                stale build dir (see 3.5)
```

Dead/legacy leftovers: `include/ui_scale.h` is a header from an earlier Qt Widgets version (comments refer to `mosaic_screen.cpp`, which does not exist). It is only included by `qml_bridge.cpp:9`, where it supplies `ui::kFontArtBody` as the fallback article font size. `AppSettings` still carries `font_name`, `dark_mode`, `card_columns`, which the QML does not use.

## 3. Build system and SDK wiring

### 3.1 Build system

Plain CMake (min 3.16) with a single `CMakeLists.txt`. No Makefile, no AUTOMOC/AUTORCC, no `qt_add_*` helpers. The build mode is selected by `-DTARGET_TYPE=ARM`; anything else is a host Linux build.

- moc is invoked manually per header in a `foreach` (`CMakeLists.txt:220-228`) for `include/qml_bridge.h`, `include/qml_models.h`, `include/i18n.h`, producing `moc_*.cpp` in the build dir.
- rcc is invoked manually twice (`CMakeLists.txt:203-217`) with `--no-zstd` (the sysroot Qt does not export `qt_resourceFeatureZstd`): `qml.qrc` to `qrc_qml.cpp`, and `res/inkfeed.qrc` to `qrc_inkfeed.cpp`.
- QML is loaded at runtime from the qrc (`qrc:/qml/Main.qml`, `main_qml.cpp:81`). It is not compiled, so no `qmlcachegen`/`qmltyperegistrar` step is needed.
- Version string: `git describe --tags` into `INKFEED_VERSION`, `INKFEED_COMMIT`, `INKFEED_BUILD_DATE` compile definitions (for the About screen). It can be overridden with `-DINKFEED_VERSION=...` (CI does this).
- Default `CMAKE_BUILD_TYPE` is `Debug` if not given (`CMakeLists.txt:52-54`); the README and CI use `Release`.
- Targets: `inkfeed` (the app), plus `inkfeed-test` on non-ARM builds only (`CMakeLists.txt:239-251`).

### 3.2 Toolchain (verified in `CMakeLists.txt:9-25`)

The toolchain is set inline before `project()`, guarded by `if(TARGET_TYPE STREQUAL "ARM")`:

- Default `SDK_PATH` = `${CMAKE_SOURCE_DIR}/../pocketbook-sdk-6.11/sdk_extracted/arm-buildroot-linux-gnueabihf_sdk-buildroot` (line 11), used only if `SDK_PATH` is not defined.
- `TOOLCHAIN_PREFIX=arm-buildroot-linux-gnueabihf`; `SYSROOT=${SDK_PATH}/${TOOLCHAIN_PREFIX}/sysroot`.
- `CMAKE_SYSTEM_NAME Linux`, `CMAKE_SYSTEM_PROCESSOR arm`, `CMAKE_SYSROOT=${SYSROOT}`.
- `CMAKE_C_COMPILER=${SDK_PATH}/bin/arm-buildroot-linux-gnueabihf-gcc`; `CMAKE_CXX_COMPILER=.../arm-buildroot-linux-gnueabihf-g++`.
- `CMAKE_FIND_ROOT_PATH=${SYSROOT}`; find modes: PROGRAM=NEVER, LIBRARY=ONLY, INCLUDE=ONLY.
- There is no toolchain file and no explicit `-march/-mfpu` flags; ABI flags come from the buildroot compiler defaults. The produced binary (`build_arm/inkfeed.app`) is `ELF 32-bit LSB executable, ARM, EABI5, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 5.10` (verified with `file`).

### 3.3 Qt and library paths (ARM branch, `CMakeLists.txt:96-131`)

- `QT6_ROOT = ${SYSROOT}/usr/local/qt6`.
- Qt is linked by absolute `.so` path: `libQt6Quick, Qml, Gui, Core, Network, Concurrent` from `${QT6_ROOT}/lib`. All six exist in the real SDK (verified). Other libraries: `pthread inkview freetype z curl xml2` (resolved from the sysroot; `libcurl`, `libxml2`, `libfreetype`, `libz` exist in `sysroot/usr/lib`, verified).
- `link_directories(${SYSROOT}/usr/local/lib)` for `libinkview.so` (exists there, verified).
- Include dirs: `${QT6_ROOT}/include` plus the `QtQml/QtQuick/QtGui/QtCore/QtNetwork/QtConcurrent` subdirs and `${SYSROOT}/usr/include/libxml2`. `inkview.h` is picked up from `sysroot/usr/local/include` (present, verified; not in an explicit include list, so it is found via the sysroot's default `/usr/local/include` search).
- Compile definition `PLATFORM_POCKETBOOK` (ARM) or `PLATFORM_LINUX` (host).
- Host moc: `find_program(HOST_MOC moc PATHS ${SDK_PATH}/local/qt6/libexec NO_DEFAULT_PATH REQUIRED)` (line 100). Verified: `local/qt6/libexec/moc` exists and is an x86-64 ELF.
- Host rcc: `find_program(HOST_RCC rcc PATHS /usr/lib/qt6/libexec /usr/lib/x86_64-linux-gnu/qt6/libexec /usr/lib/qt6/bin REQUIRED)` (lines 104-106). The SDK also ships `local/qt6/libexec/rcc`, but a code comment says it is dynamically linked against an ICU that the SDK does not ship. `readelf -d` shows it needs `libQt6Core.so.6`; the ICU claim is **unverified** (not executed).
- Output: `build_arm/inkfeed.app` (`SUFFIX ".app"`, `OUTPUT_NAME inkfeed`, line 232). Note this "`.app`" is the ELF itself, not a bundle.

### 3.4 Verified state of this machine (important for reproduction)

Checked with `command -v` / `ls`:

- The real SDK is at `$POCKETBOOK_SDK` (no `sdk_extracted/` level; `ls pocketbook-sdk-6.11/sdk_extracted` fails). So the default `SDK_PATH` does **not** resolve here and **`-DSDK_PATH` must be passed**.
- `gcc`/`g++`/`make`/`zip` are installed on the host. `/usr/bin/cmake` exists (version 4.2.3; an earlier `command -v` check wrongly reported it missing). `/usr/bin/ninja` also exists. Note CMake 4.x removed compatibility with `cmake_minimum_required` below 3.5; this project requires 3.16, so that is fine.
- **No host `rcc` was found** in the paths CMake searches: `/usr/lib/qt6/libexec` contains only `QtWebEngineProcess` and `webenginedriver` (`/usr/lib/qt6/bin` and `/usr/lib/x86_64-linux-gnu/qt6/libexec` do not exist). A full-disk `find / -name rcc -type f` found only the SDK's `local/qt6/libexec/rcc` and a snap copy (`/snap/kf6-core24/64/usr/lib/qt6/libexec/rcc`, outside the searched paths). Qt 6.10.2 runtime libs are present in `/usr/lib/x86_64-linux-gnu`, but no Qt dev tools. Without an rcc in those paths, `cmake` configure fails at `find_program(HOST_RCC ... REQUIRED)`.
- The SDK ships its own `local/qt6/libexec/rcc`. Using it was the author's rejected path (ICU); whether it actually runs here was **not tested**.
- Qt version note: the SDK sysroot Qt is 6.8 (per the CMake comment); the host Qt is 6.10.2. The rcc output must be compatible across 6.x (author's claim; hence `--no-zstd`).

### 3.5 Stale `build_arm/`

`<InkFeed checkout>/build_arm/` is root-owned and was generated in a container: `CMakeCache.txt` says "For build in directory: /src/build_arm", `CMAKE_HOME_DIRECTORY=/src`, generator Ninja, `SDK_PATH:UNINITIALIZED=/opt/pocketbook-sdk/sdk_extracted/arm-buildroot-linux-gnueabihf_sdk-buildroot`, `TARGET_TYPE=ARM`, `CMAKE_BUILD_TYPE=Release`, CMake 3.28.3. It contains the finished `inkfeed.app` (1,792,484 bytes), identical in size to `applications/inkfeed.bin` in `dist/inkfeed-inkpad-one-1.0.0-2-g901306f.zip`. It must not be reused as a build dir (wrong source dir and SDK path, root-owned); configure into a fresh directory.

## 4. Application architecture

Not a classic InkView event loop. `main()` (`src/main_qml.cpp:57-108`):

1. Creates a `QGuiApplication` (so the Qt event loop `app.exec()` drives everything).
2. Chooses the data dir: `USERDATA "/inkfeed"` (= `/mnt/ext1/system/inkfeed`) on device; `./inkfeed-data` on desktop.
3. `inkfeed::App core; core.init(data_dir)` (opens SQLite, creates feed manager, sync engine, image cache, loads settings; `src/core/app.cpp`).
4. `i18n::install(...)`, constructs `QmlBridge bridge(core)`, exposes it as the QML context property `bridge`, and loads `qrc:/qml/Main.qml` with `QQmlApplicationEngine`.
5. Optional offscreen hook: if `INKFEED_QML_SHOT` is set, it renders a 1404x1872 PNG and quits (desktop verification).

The platform plugin is chosen by the launcher, not by code: `QT_QPA_PLATFORM=pocketbook2egl` (see section 8). The dev notes (`docs/inkpad-one-dev-notes.md`) state that Qt Quick must use `pocketbook2egl` with the default RHI/OpenGL backend; `QT_QUICK_BACKEND=software` gives a blank panel and the raster `pocketbook2` QPA aborts on RHI init. I did not verify this on a device.

Layers: QML UI (`Main.qml`) <-> `QmlBridge` (Q_PROPERTY/Q_INVOKABLE, `include/qml_bridge.h`) <-> `inkfeed::App` core (feed manager, sync engine, DB, HTTP, image cache). Sync runs on worker threads via `QtConcurrent::run` (`qml_bridge.cpp:156`).

### 4.1 How InkView is still used (glue only)

`inkview.h` is included under `PLATFORM_POCKETBOOK` in `main_qml.cpp`, `qml_bridge.cpp`, `core/app.cpp`, `core/feed_manager.cpp`, `core/i18n.cpp`, `core/sync_engine.cpp`, `utils/http.cpp`. The InkView calls found:

- Clean exit (`qml_bridge.cpp:484-494`): `ClearScreen(); FullUpdate(); CloseApp();` before `QCoreApplication::quit()` to avoid a long black flash.
- Rotation sensor (`qml_bridge.cpp:87-92, 860-863`): `SetGSensorEnabled(true)`, then a 500 ms `QTimer` polls `GetGSensorOrientation()`. The value is mapped to 0/90/180/270 degrees and exposed to QML as `uiRotation`. InkView's `SetOrientation()` is deliberately not used.
- Task icon (`main_qml.cpp:31-54, 90-91`): `LoadBitmap`/`LoadImageToFormat`, `GetCurrentTask`, `GetTaskInfo`, `SetTaskParameters`, re-applied at 1.2 s, 3.5 s, 8 s after start so the running-apps switcher shows the InkFeed icon.
- Open URL (`qml_bridge.cpp:68-70`): `OpenBook(BROWSER_FOR_AUTH, url, 0)` launches the system browser (`QDesktopServices` is a no-op on device).
- Paths and I/O: `USERDATA` macro (data dir), `FLASHDIR` (OPML import/export dir, `qml_bridge.cpp:686, 704`), `iv_fopen/iv_fread/iv_fwrite` for OPML files (`feed_manager.cpp:97-118`).
- System info: `ScreenWidth()/ScreenHeight()` (`app.cpp:34-37`), `currentLang()` for the UI language (`i18n.cpp:46`), `GetGlobalConfig()/ReadInt(cfg,"reader.font.size")` and `"textScale"` for the baseline article font size (`qml_bridge.cpp:159-172`).
- HTTP download of files via inkview sessions: `NewSession`, `SetUserAgent`, `DownloadTo`, `GetSessionInfo`, `AbortTransfer`, `NetError` in `utils/http.cpp`, used only by `download_file()`. `HttpClient::get()` uses libcurl.
- `sync_engine.cpp` includes `inkview.h` (line 10-11) but the specific calls there were not examined.

No `InkViewMain`, no `main_handler(int type, int par1, int par2)` event handler, no `Repaint/SoftUpdate/PartialUpdate` calls were found.

### 4.2 Screens (all in `qml/Main.qml`)

Single `ApplicationWindow`, `Window.FullScreen`, white background. A `StackView` (`nav`) with all transitions removed (`Main.qml:174-181`, "instant swaps suit e-ink") holds pages: Home (mosaic card grid per collection, tabs for collections, sync button), Article reader, Settings, Manage Feeds, Bookmarks (with search). Overlays inside the rotating stage: modal picker, message dialog, text prompt, app menu.

### 4.3 Rotation, DPR, scale

The stage `Item` (`Main.qml:157-172`) is sized to `window x Screen.devicePixelRatio` and given `scale: 1/dpr` and `rotation: bridge.uiRotation`, swapping width/height when sideways. This counters the ~1.30 devicePixelRatio of `pocketbook2egl` (logical 1080x1440 over a 1404x1872 panel, per the dev notes) so that sizes authored in panel pixels are 1:1. Rotation preference (`auto`/`portrait`/`landscape`) is a setting; `portrait` forces 0, `landscape` forces 90 (`qml_bridge.cpp:850-858`). The framebuffer stays portrait.

### 4.4 Fonts

No `FontLoader`, no custom font family, no `font.family` setting in QML: all text uses the device's default Qt font. Sizes are fixed pixel sizes (`font.pixelSize: 24..56`). Article body size is `bridge.articleFontPx` (`Main.qml:718`), initialised from the firmware reader setting `reader.font.size` x `textScale` (`qml_bridge.cpp:159-172`) or a default of 38 px (`ui::kFontArtBody`), persisted as `article_font_px`. The README advertises pinch-to-zoom, and `QmlBridge::setArticleFontPx/resetArticleFont` exist (`qml_bridge.cpp:373-382`), but **no `PinchHandler`/`PinchArea` and no QML caller of `setArticleFontPx` were found** in `Main.qml`. The font-zoom UI path therefore appears not to be wired in this version **(unverified at runtime)**. Unicode glyphs used as icons (`☰ ↗ ← ↻ ✕ ✓ ★`) rely on the device UI font (dev notes list which render).

### 4.5 Touch

QML pointer handling only: `MouseArea` (rows, menus, overlays) and `TapHandler` (cards); `Flickable` with `boundsBehavior: Flickable.StopAtBounds` for scrolling. No inkview touch events, no gesture handlers found. Buttons are `QtQuick.Controls` `Button` restyled flat by `InkButton.qml` (white, 2 px border, `#d8d8d8` when pressed, no animations).

### 4.6 Keyboard

Native Qt Quick `TextField` inside a custom modal prompt (`Main.qml:295-355`): `forceActiveFocus()` then `Qt.inputMethod.show()`; numeric input via `inputMethodHints = Qt.ImhDigitsOnly`; `Qt.inputMethod.hide()` on accept/cancel. InkView `OpenKeyboard` is not used. The dev notes state the system on-screen keyboard appears over the EGL surface with this approach (not independently verified).

### 4.7 Theme / dark mode

The settings struct has `theme` ("auto"/"light"/"dark"), but `Main.qml` uses hard-coded light colours (e.g. `#fff`, `#000`, `#222`) and no theme switch was found. The README claims "light/dark following the reader", and the dev notes describe `screen_mode_inverted` in `/mnt/ext1/system/config/global.cfg` as the system flag, but `grep` found **no use of `screen_mode_inverted` or any dark-theme handling in the code**. Treat dark mode as not implemented in this snapshot.

## 5. Networking, HTTPS, storage, configuration

### 5.1 HTTP

- Feeds, article pages and discovery: libcurl via `HttpClient::get()` (`src/utils/http.cpp:94-151`) on all platforms. Options: `NOSIGNAL`, 10 s connect timeout, caller-supplied total timeout, `FOLLOWLOCATION`, `ACCEPT_ENCODING ""` (gzip), UA `inkfeed/1.0`, conditional GET (`If-None-Match`/`If-Modified-Since`, ETag/Last-Modified captured).
- The comment explains that inkview `Download()` returns 0 bytes / hangs on this firmware, so it is not used for `get()`.
- Images: `ImageCache` (`src/utils/image_cache.cpp`) uses `QNetworkAccessManager` with `NoLessSafeRedirectPolicy`. Its HTTPS behaviour on device depends on Qt's TLS backend and CA store; not examined and **unverified**.
- Feed discovery uses Feedly's public search API (README).

### 5.2 TLS / CA bundle

**There is no `cacert.pem` and no CA bundle path anywhere in the repository** (grep for `cacert|CAINFO|ca-bundle|.pem` found nothing). Instead, on device builds the code disables verification (`http.cpp:131-136`):

```cpp
#ifdef PLATFORM_POCKETBOOK
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
```

with the comment "The device ships no usable CA bundle (inkview's own downloader skips verification too)". On desktop, default curl verification applies. The SDK sysroot has an empty `etc/ssl/certs` directory (verified). Whether the device itself has a usable bundle was not checked.

### 5.3 Storage and configuration

- Data dir `/mnt/ext1/system/inkfeed/` on device (`main_qml.cpp:64`); files: `inkfeed.db` (SQLite), `img-cache/` (images keyed by SHA-1 of the URL), `task-icon.bmp`, and `inkfeed.log` (written by the launcher). Desktop: `./inkfeed-data`.
- SQLite is the vendored amalgamation (`lib/sqlite3.c`). `PRAGMA journal_mode=TRUNCATE`, `busy_timeout=5000`, `foreign_keys=ON` (`database.cpp:24`) because `/mnt/ext1` is vfat and WAL's shm is unreliable there (dev notes).
- Settings are key/value rows in the same DB (`load_settings/save_settings`, `database.cpp:121-122`): font_size, font_name, show_unread_only, sync_interval_hours, max_articles_per_feed, dark_mode, card_columns, language, theme, load_images, fetch_full_articles, article_font_px. The rotation preference is also read by `QmlBridge`. Settings are changed via the in-app Settings page; there is no config file.
- OPML import/export uses `FLASHDIR` (`/mnt/ext1`): exports `inkfeed-subscriptions.opml`, imports `*.opml`/`*.xml` from the same dir.
- i18n: runtime JSON translator (`res/i18n/{nl,es,fr,de}.json` embedded in the qrc; English is the source text). Language follows `currentLang()` unless overridden.
- Parsing: RSS/Atom/OPML via libxml2; reader mode is a custom libxml2 heuristic in `src/core/readability.cpp` (a feed body shorter than `kAmpleChars = 2600` visible characters triggers a full-page fetch).

## 6. E-ink specifics (as actually found)

- **Page transitions disabled**: all `StackView` transitions are empty (`Main.qml:178-181`), no `Behavior`/`NumberAnimation` found in the QML.
- **Flat, high-contrast styling**: `InkButton.qml` (white fill, 2 px border, grey on press), black/white text, `boundsBehavior: StopAtBounds` (no overscroll bounce).
- **Clean-exit refresh**: `ClearScreen(); FullUpdate(); CloseApp();` (`qml_bridge.cpp:490-494`) to avoid a black panel after quit.
- **Pixel-exact rendering**: DPR compensation (4.3), icons as opaque BMPs.
- **Rotation without InkView**: content rotation inside the Qt surface.
- **Refresh strategy / partial updates / waveform modes / ghosting control**: **not found**. There are no calls to `PartialUpdate`, `SoftUpdate`, `DynamicUpdate`, `FullUpdate` (except on exit), `SetEPDMode` or similar, and no periodic full-refresh logic in the C++, QML or launcher. Panel updates are left entirely to the `pocketbook2egl` QPA / firmware. Whether it uses partial or full updates per frame is **unknown from this repo**.

## 7. Entry points that matter for a port

- Binary names: build output `inkfeed.app` (ELF); on device the ELF is installed as `inkfeed.bin` and a shell script is installed as `inkfeed.app` (naming trap, see section 8).
- Everything outside `qml/` and the `#ifdef PLATFORM_POCKETBOOK` blocks compiles and runs on desktop Linux (the host build produces a working app plus `inkfeed-test`).
- The Qt libs are linked against `/usr/local/qt6` paths baked from the SDK; the device has Qt in `/usr/lib` and QML modules in `/usr/qml`, hence the launcher's env overrides. Because only `Quick/Qml/Gui/Core/Network/Concurrent` are linked, `QtQuick.Controls` and `QtQuick.Layouts` imports are resolved from the device's `/usr/qml` at runtime (dev notes). Whether the device's Qt minor version matches the SDK's 6.8 was not checked.

## 8. Packaging and installation

### 8.1 Launcher (`platform/pocketbook/inkfeed.app`, 31 lines, POSIX `sh`)

Installed at `/mnt/ext1/applications/inkfeed.app`. It:
- creates `/mnt/ext1/system/inkfeed`, redirects stdout/stderr to `inkfeed.log` there;
- exports `LD_LIBRARY_PATH="/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"`, `QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms`, `QT_PLUGIN_PATH=/usr/plugins`, `QT_QPA_PLATFORM=pocketbook2egl`, `QML_IMPORT_PATH` and `QML2_IMPORT_PATH=/usr/qml`;
- `cd /mnt/ext1` and runs `/mnt/ext1/applications/inkfeed.bin`, logging the exit code.

### 8.2 Packaging script (`scripts/package-inkpad-one.sh <built-arm-elf> [version] [out-dir]`)

Stages a temp dir and zips it (needs `zip`, optionally `git`):
- `install -m 777 $BIN applications/inkfeed.bin`
- `install -m 777 platform/pocketbook/inkfeed.app applications/inkfeed.app`
- `res/desktop_app_inkfeed.bmp` -> `applications/icons/inkfeed.bmp`; `res/desktop_app_inkfeed_f.bmp` -> `applications/icons/inkfeed_f.bmp` (mode 666)
- `docs/install.md` -> `INSTALL.md`
- Output: `<out-dir>/inkfeed-inkpad-one-<version>.zip`, default out-dir `dist` (relative to the current directory), version defaults to `git describe --tags --always` without the leading `v`.

The existing `dist/` zip contains exactly: `INSTALL.md`, `applications/inkfeed.app` (1306 B), `applications/inkfeed.bin` (1,792,484 B), `applications/icons/inkfeed.bmp`, `applications/icons/inkfeed_f.bmp`.

### 8.3 Installation on the device (`docs/install.md`)

1. Unzip; copy `applications/` into the root of the reader's storage (`/mnt/ext1`), merging.
2. Register in the Applications menu by editing the user config `system/config/desktop/view.json` (back it up first): add under top-level `"applications"`:
   ```json
   "U_InkFeed": {
     "path": "/mnt/ext1/applications/inkfeed.app",
     "title": "InkFeed",
     "icon": { "path": "applications/icons/inkfeed.bmp" },
     "focused_icon": { "path": "applications/icons/inkfeed_f.bmp" }
   }
   ```
   and add `"U_InkFeed"` to one of the `"groups"` app lists (e.g. `@General`). Gotchas from the dev notes: id must start with `U_`, `path` must be absolute, icons are opaque BMPs (122x122, 8bpp), do not rely on `apps.json`.
3. Reboot; start from Applications -> General.

Dev-loop (dev notes, device-specific, not verified by me): `/` is read-only, an SSH server can be provided by a KOReader install (port 2222); upload the binary to a temp name and `mv` it over `inkfeed.bin` to avoid `ETXTBSY`. Tested only on the InkPad One (RK3566, aarch64 kernel 5.10 with 32-bit armhf userland); other models untested.

CI: `.github/workflows/inkpad-one.yml` downloads the SDK from a `POCKETBOOK_SDK_URL` repo variable (7z), locates `*_sdk-buildroot`, configures with `-DSDK_PATH`, builds, and checks `file build_arm/inkfeed.app` for `ELF 32-bit.*ARM`. `release.yml` (not examined in detail) builds on `v*` tags. `linux.yml` builds with Ninja and runs `inkfeed-test`.

## 9. Build commands to reproduce (derived, NOT executed)

### 9.1 Prerequisites

- Host: `cmake` >= 3.16 (3.28.3 was used for the existing cache; 4.2.3 is installed here), a build tool (Ninja or Make; the stale cache used Ninja), `zip`, `git` (optional; without git, version falls back to `0.0.0`/the argument).
- Host Qt `rcc` reachable at one of `/usr/lib/qt6/libexec`, `/usr/lib/x86_64-linux-gnu/qt6/libexec`, `/usr/lib/qt6/bin` (on Debian/Ubuntu from the package `qt6-base-dev-tools`, as installed by `qt6-base-dev`; package name is from memory, **unverified**). On this machine no rcc exists in those paths (section 3.4); cmake 4.2.3 and ninja are present. Install `qt6-base-dev-tools` to provide `rcc`.
- The PocketBook SDK 6.11 (RK3566) extracted. Here: `$POCKETBOOK_SDK`. The SDK's x86-64 host tools (cross `gcc`, `moc`) must be runnable on this host.
- The SDK may need its `relocate-sdk.sh` to have been run (it exists in the SDK root); whether it already was is **unverified**. The compiler was not executed.

### 9.2 Configure, build, package

Run from `<InkFeed checkout>`, using a fresh build dir (not the stale `build_arm`). Since `InkFeed` is meant to be read-only for this analysis, build out-of-tree, e.g. in a scratch dir:

```bash
SDK="$POCKETBOOK_SDK"
SRC=/path/to/InkFeed
BLD=/path/to/new/build_arm            # must be empty / new

# configure (TARGET_TYPE=ARM selects the toolchain block; SDK_PATH override is required here)
cmake -S "$SRC" -B "$BLD" -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release -DSDK_PATH="$SDK"
# optionally add: -G Ninja   and   -DINKFEED_VERSION=<string>

# build
cmake --build "$BLD" -j"$(nproc)"      # -> $BLD/inkfeed.app (32-bit ARM ELF)

# sanity check
file "$BLD/inkfeed.app"                # expect: ELF 32-bit LSB executable, ARM, EABI5

# package (3rd arg = output dir; default is ./dist relative to cwd)
"$SRC/scripts/package-inkpad-one.sh" "$BLD/inkfeed.app" "" "$BLD/dist"
#   or with an explicit version:  ... "$BLD/inkfeed.app" 1.0.0-local "$BLD/dist"
```

Notes:
- `-DSDK_PATH` is mandatory in this checkout layout; the default expects `../pocketbook-sdk-6.11/sdk_extracted/...`, which does not exist here.
- The packaging script's second argument is the version; passing an empty string `""` is treated as unset by `${2:-...}` and falls back to `git describe`, then `0.0.0`.
- The in-tree commands from the README (`cmake -S . -B build_arm ...`) would write into the InkFeed directory; the `build_arm` that already exists there is root-owned and stale.
- Desktop dev build (not for the device), per README: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/inkfeed-test`, needing `qt6-base-dev qt6-declarative-dev libfreetype-dev zlib1g-dev libcurl4-openssl-dev libxml2-dev`.

## 10. Unverified / not found summary

- Not found: any CA bundle (`cacert.pem`), TLS verification on device (disabled instead), explicit e-ink refresh/partial-update/waveform control, InkView event loop, dark-theme implementation, pinch-to-zoom wiring in QML.
- Unverified: whether the SDK's own `rcc` runs on this host (ICU claim), whether the SDK has been relocated and its cross-compiler runs, device Qt minor-version compatibility with the SDK's Qt 6.8, behaviour of `ImageCache` TLS on device, the exact calls in `sync_engine.cpp` and the contents of `release.yml`, and all device behaviours quoted from `docs/inkpad-one-dev-notes.md` (taken from the author's notes, not tested here).
