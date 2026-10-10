# PocketBook SDK analysis

Analysis date: 2026-10-10. SDK root (read-only, nothing was modified):

```
$POCKETBOOK_SDK
```

Below, `$SDK` means that directory and `$SYSROOT` means `$SDK/arm-buildroot-linux-gnueabihf/sysroot`.
Everything stated here was checked with `ls`, `find`, `grep`, `readelf`, `strings`, or by running a
tool from the SDK. Items that are inferred or could not be verified are marked **[UNVERIFIED]** or **[INFERRED]**.

## 1. Toolchain, architecture, sysroot, version

| Item | Value | Evidence |
|---|---|---|
| Compiler prefix | `arm-buildroot-linux-gnueabihf-` | `$SDK/bin/arm-buildroot-linux-gnueabihf-{gcc,g++,ar,ld,strip,...}`; `gcc -v` reports `Target: arm-buildroot-linux-gnueabihf` |
| Compiler | GCC 15.2.0 (Buildroot `-g7c7772441`), languages c,c++ | `gcc -v` |
| Wrappers | `-gcc`, `-g++` are Buildroot wrappers that call `*.br_real` (x86-64 host ELF) | `file bin/...-gcc.br_real` |
| Target architecture | 32-bit ARM, hard-float EABI5 (armhf), default `-mcpu=cortex-a55 -march=armv8.2-a+simd -mfpu=neon -mfloat-abi=hard`, ARM mode (not Thumb by default) | `gcc -v` configure line (`--with-cpu=cortex-a55 --with-fpu=neon --with-float=hard --with-mode=arm`), `gcc -Q --help=target`, `readelf -A libinkview.so` (Tag_CPU_arch v8, 8.2-A, NEON) |
| CMake system processor (toolchain file) | `armv8l` | `share/buildroot/toolchainfile.cmake` |
| Sysroot | `$SDK/arm-buildroot-linux-gnueabihf/sysroot` (glibc, dynamic loader `ld-linux-armhf.so.3`) | directory exists; `libc.so.6`, `libstdc++.so.6.0.34` present |
| Build host of the SDK | x86-64 Linux (host tools such as `moc`, `cmake`, `ninja` are x86-64 ELF) | `file` |
| Smoke test | `bin/arm-buildroot-linux-gnueabihf-gcc t.c -o t.out` produced `ELF 32-bit LSB executable, ARM, EABI5` | compiled with a trivial `main`, output kept in the session scratchpad only |

The compiler works in place at the current location even though `relocate-sdk.sh` was never run: 
`share/buildroot/sdk-location` still says `/BUILD/arm-buildroot-linux-gnueabihf_sdk-buildroot`. The
wrappers and the toolchain file compute their paths relative to their own location, so this is harmless
for GCC/CMake. The `.pc` files under the sysroot still contain the absolute `/BUILD/...` prefix (see section 7).

### SDK version: what evidence exists

There is **no version marker file** inside the SDK (no `VERSION`, `README`, `release`, `sdk-version` file;
`doc/` contains only `cmake-3.31`; `inkview.h` has no `SDK_VERSION`-style macro; `libinkview.so` has no
embedded "6.11" string; the cmake package `inkview` has `PACKAGE_VERSION "0.0.1"`, which is not the SDK version).

Evidence that does exist:

1. The enclosing directory name `pocketbook-sdk-6.11` (given by whoever extracted it) and
   InkFeed's `README.md:58` ("SDK 6.11 (RK3566) release from Sean-on-Git/PocketBook-SDK"), and the CI cache key
   `pbsdk-6.11-rk3566`. This is the only source of the "6.11" label. **[UNVERIFIED against the SDK contents]**
2. GCC 15.2.0, clang/LLVM 20.1.8 (`.comment` of `libinkview.so`: `GCC: (Buildroot -g7c7772441) 15.2.0`, `clang version 20.1.8`), Qt 6.8.2, OpenSSL 3.5.4,
   curl 8.16.0, CMake 3.31.8. These identify the Buildroot snapshot, not the PocketBook SDK number.
3. The GCC configure line shows it was built on a Jenkins job `BSP-rockchip/buildroot/output/rockchip_1030`;
   `$SDK/workdir/system` and `$SDK/local/share/config/hwconfig_1030` exist, and `hwconfig.cfg` has
   `device_number=1030`, `device_platform=18`, `device_display_width=1404`, `device_display_height=1872`, `device_display_dpi=227`.
   So the SDK targets the Rockchip platform with device number 1030 (display 1404x1872 at 227 dpi, which matches
   the InkPad One class of device; the mapping to the product name is **[INFERRED]**).
4. File mtimes: toolchain/host files 2025-11-18, `bin/`, `lib/`, `local/` and `libinkview.so` 2026-02-12. So the SDK was built in
   Feb 2026 at the latest.

Conclusion: "6.11" is a label, not verifiable from inside the tree. The content is consistent with a Rockchip (RK3566-class,
Cortex-A55, armhf userland) SDK built from a Buildroot 2025.x snapshot.

## 2. inkview.h and libraries

### PocketBook-specific files (`$SYSROOT/usr/local`)

| Path | Notes |
|---|---|
| `usr/local/include/inkview.h` | 140,951 bytes. Also `inkplatform.h`, `inkinternal.h`, `inklog.h`, `hwconfig.h`, `scrollview.h`, `selection_list.h`, `GTypes.h`, `P7_*.h`, `inkview/{accessibility_*,global_gesture,screen_reader,sleep_preventor}.h`, `common_utilities/`, `gtest/`, `gmock/`, `gsl/`, `tl/` |
| `usr/local/lib/libinkview.so` | 1,477,800 bytes, `ELF 32-bit LSB shared object, ARM, EABI5`, not stripped. NEEDED: libssl.so.3, libcrypto.so.3, libfreetype.so.6, libjpeg.so.8, libtiff.so.6, libpng16.so.16, libz.so.1, libcurl.so.4, libicuuc/icudata/icui18n/icuio.so.73, libjsoncpp.so.26, libhwconfig.so, libstdc++, libc |
| `usr/local/lib/` other | `libhwconfig.so` (+ static), `libcommon_utilities.so`, `libp7.a`, `libgtest*.so.1.15.0`, `libgmock*.so.1.15.0` |
| `usr/local/lib/cmake/` | CMake packages `inkview`, `hwconfig`, `hwconfig.static`, `common_utilities`, `p7`, `GTest` (inkview package version 0.0.1) |
| `usr/local/bin/` | target-side helper binaries (`hwconfig.app`, `power_off_logo`, `dosfsck.app`, ...) - ARM binaries for the device, not host tools |
| `usr/local/qt6/` | Qt 6.8.2 for the target (section 3) |

`$SDK/local/` is a second tree (x86-64 host side): `local/qt6/libexec/moc`, `local/qt6/bin/*` (qmake, qmlcachegen, lupdate, ...), and host copies of headers/libs. `$SDK/local/bin/hwconfig.app` is x86-64.

`inkview.h` is not conditional on any SDK-version macro; the closest version-related APIs are runtime functions
(`GetSoftwareVersion()`, `IvReadVersionFromFirmwareFile()`), which report firmware, not SDK, version.

### Other libraries in the sysroot (`$SYSROOT/usr/lib`, headers in `usr/include`)

Versions are from the `.pc` files or headers where marked.

| Requested | Present? | Detail |
|---|---|---|
| curl | yes | `libcurl.so.4.8.0`, version 8.16.0 (`libcurl.pc`), headers `usr/include/curl/` |
| ssl / crypto | yes | `libssl.so.3`, `libcrypto.so.3`, OpenSSL 3.5.4 (`opensslv.h`), `usr/include/openssl/` |
| sqlite | yes | `libsqlite3.so.0.8.6`, 3.48.0, `sqlite3.h` |
| json | yes (multiple) | `libjson-c.so.5` (json-c 0.18), `libjsoncpp.so.26` (1.9.6), also `libjson.so.7`; headers `json-c/`, `json/`, `libjson/`; `libjq.pc` exists too |
| freetype | yes | `libfreetype.so.6.20.2` (freetype2.pc "26.2.20"), `usr/include/freetype2/` |
| iconv | partial | no separate `libiconv`; `usr/include/iconv.h` is glibc's, and `usr/lib/gconv/` has the glibc converter modules. ICU 73 is also present (`libicu*.so.73`) |
| harfbuzz | no system lib | no `libharfbuzz*`, no `hb.h` in `usr/include`. Only Qt's private bundled copy: `usr/local/qt6/include/QtHarfbuzz/harfbuzz/hb.h` (Qt6HarfbuzzPrivate) |
| zip | no | no `libzip`, no `zip.h`, no minizip found (`find` for `zip.h`, `libzip*`, `libminizip*` was empty). Only zlib 1.3.1 (`libz.so.1.3.1`, `zlib.h`), plus `libbz2`, `liblzma` (5.8.1) |
| png | yes | `libpng16.so.16.50.0`, 1.6.50 |
| jpeg | yes | `libjpeg.so.8.3.2` (libjpeg-turbo 3.1.0), `jpeglib.h`, `turbojpeg.h` |
| xml | yes | `libxml2.so.2.13.8` (2.13.8), headers `usr/include/libxml2/`; also `libexpat.so.1.11.0` |
| z | yes | zlib 1.3.1 |
| others seen | - | libtiff 6, libwebp 1.5.0, libgif 7, fontconfig, libpcre2-8, libnghttp2, c-ares, libpsl, ncurses 6.5, ffmpeg libs (libav*), mpg123, FLAC, opus, vorbis, OpenCV 4 (`opencv4.pc`), boost headers, LLVM 20 / clang 20 libs (very large) |

## 3. Qt6

- Location (target): `$SYSROOT/usr/local/qt6` with `lib/`, `include/`, `plugins/`, `qml/`, `mkspecs/`, `lib/cmake/Qt6*`.
- Version: **6.8.2** (`#define QT_VERSION_STR "6.8.2"` in `usr/local/qt6/include/QtCore/qconfig.h`; `libQt6Core.so.6.8.2`).
- Host side: `$SDK/local/qt6` (x86-64). `local/qt6/libexec/moc --version` prints `moc 6.8.2` and runs on this machine.
- Modules present as shared libs include Core, Gui, Network, Concurrent, DBus, Qml, Quick, QuickControls2, Widgets-related libs, Pdf, Core5Compat, Help, Designer (`ls lib` and `lib/cmake`; only the first few were individually listed).
- QPA platform plugins (`plugins/platforms`): `libqpocketbook2.so` (PocketBook plugin, links against libinkview, build path string `/BUILD/build/pocketbook2/EinkEventHandler.cpp`), `libqlinuxfb.so`, `libqminimal.so`, `libqoffscreen.so`, `libqvnc.so`.
- Host `rcc` problem: `$SDK/local/qt6/libexec/rcc` fails here with `libicui18n.so.76: cannot open shared object file` (needs ICU 76 which the SDK does not ship; the target sysroot has ICU 73). Verified by running it. `uic` and `lrelease` were not tested; InkFeed's notes say `lrelease` is also unusable. `moc` works.
- Host Qt `cmake` helpers: `local/qt6/lib/cmake/Qt6/Qt6Config.cmake` exists, and the toolchain file sets `QT_HOST_PATH` to the SDK root. A full `find_package(Qt6)` cross-build through the SDK toolchain file was **not** attempted **[UNVERIFIED]**.

## 4. CMake toolchain and environment files

- `$SDK/share/buildroot/toolchainfile.cmake` (auto-generated by Buildroot). Sets: `CMAKE_SYSTEM_NAME Buildroot`, `CMAKE_SYSTEM_PROCESSOR armv8l`, `CMAKE_SYSROOT`/`CMAKE_FIND_ROOT_PATH` to `<SDK>/arm-buildroot-linux-gnueabihf/sysroot`,
  compilers `<SDK>/bin/arm-buildroot-linux-gnueabihf-{gcc,g++}`, default flags `-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Os -g0 -D_FORTIFY_SOURCE=1`, find modes (programs NEVER, libs/includes/packages ONLY),
  `PKG_CONFIG_SYSROOT_DIR`, and `QT_HOST_PATH=<SDK>`, `QT_HOST_PATH_CMAKE_DIR=<SDK>/lib/cmake`. It derives `<SDK>` from its own location, so it is relocatable.
  Also `share/buildroot/Platform/` (custom CMake platform description for system name "Buildroot") and `share/buildroot/sdk-location`.
- `$SDK/environment-setup`: source it in bash/zsh. Exports CC/CXX/AR/LD/... with the cross prefix, `CFLAGS`/`CXXFLAGS`, `ARCH=arm`, `CROSS_COMPILE=arm-buildroot-linux-gnueabihf-`, `STAGING_DIR=$SDK_PATH/arm-buildroot-linux-gnueabihf/sysroot`, `CONFIGURE_FLAGS`, a `cmake` alias that passes `-DCMAKE_TOOLCHAIN_FILE=.../toolchainfile.cmake -DCMAKE_INSTALL_PREFIX=/usr`, and prepends `$SDK/bin` and `$SDK/sbin` to `PATH`. It does not set any Qt variables or `PKG_CONFIG_PATH`.
- `$SDK/relocate-sdk.sh`: standard Buildroot relocation script (rewrites `/BUILD/...` paths). Not executed; it would modify the SDK, so it was deliberately not run.
- The SDK ships its own host `bin/cmake` (3.31.8), `bin/ninja` (1.11.1), `bin/pkg-config`/`pkgconf`, plus `bin/` LLVM/clang tools (479 entries in `bin/`). The system `PATH` of this machine has `cmake` (4.2.3) and `ninja` in `/usr/bin` but no Qt `rcc` (rechecked 2026-10-10; an earlier check wrongly reported cmake and ninja as missing).
- Not present: Qt-specific toolchain file (e.g. `qt.toolchain.cmake`) was not looked for exhaustively **[UNVERIFIED]**; `local/qt6/bin/qt-cmake` exists as a wrapper.

## 5. Sample projects and simulator/emulator

- Sample projects: **none found.** `find` for `*sample*`, `*example*`, `*demo*` outside Qt/LLVM/include/share produced only third-party leftovers (ffmpeg examples, dbus/mpfr/pcre2 docs, Python tests). No PocketBook sample app, no template CMakeLists, no `README`. The only documentation is `$SDK/doc/cmake-3.31`.
- Simulator/emulator: **none found.** No `qemu*` binaries, nothing named `simulator`/`emulator`, no x86-64 `libinkview` (the only `libinkview.so` is ARM). The `llvm-sim` hit in `bin/` is an LLVM similarity tool, unrelated. The Qt `offscreen`/`minimal`/`vnc` platform plugins exist but are target (ARM) builds; running anything therefore requires the device or a user-space ARM emulator that is not part of the SDK **[UNVERIFIED: qemu-user availability on host not checked]**.
- `$SDK/local/bin/hwconfig.app` and `$SYSROOT/usr/local/bin/hwconfig.app` are config utilities, not an emulator.

## 6. Extraction oddities

- `$SDK/usr` is a **0-byte regular file** (mtime 2026-10-10 08:43), and `$SDK/workdir/system` is also a 0-byte regular file with the same timestamp. In a Buildroot SDK `usr` is normally a symlink (typically to `.`), and InkFeed's CI comment says 7-Zip refuses the archive's absolute/parent symlinks (naming `udevadm` and `workdir/system`). **[INFERRED]** these are placeholders left by an extractor that could not create the symlinks. Consequence: the `.pc` files' `/BUILD/.../usr/arm-buildroot-linux-gnueabihf/sysroot/...` prefix cannot resolve even after relocation. This does not matter for InkFeed (it does not use pkg-config on ARM) but would matter for pkg-config based builds. Other dangling/placeholder symlinks were not enumerated **[UNVERIFIED]**.

## 7. Compatibility with InkFeed (<InkFeed checkout>, read-only)

### 7.1 Version/feature compatibility

| InkFeed expectation | SDK reality | Result |
|---|---|---|
| "SDK 6.11 (RK3566)" (`README.md:58`, CI key `pbsdk-6.11-rk3566`) | Directory is `pocketbook-sdk-6.11`; internal evidence consistent with Rockchip/armhf/Cortex-A55; no marker confirms "6.11" | Consistent, not provable |
| Compiler `arm-buildroot-linux-gnueabihf-{gcc,g++}` at `$SDK/bin` (`CMakeLists.txt:13,19-20`) | Present, GCC 15.2.0 | Match |
| Sysroot `$SDK/arm-buildroot-linux-gnueabihf/sysroot` (`CMakeLists.txt:14`) | Present | Match |
| Qt at `$SYSROOT/usr/local/qt6`, "Qt 6.8" (`CMakeLists.txt:93,97`) | Qt **6.8.2** at exactly that path | Match. InkFeed's ARM branch has no `find_package(Qt6 <version>)`; it links `libQt6{Quick,Qml,Gui,Core,Network,Concurrent}.so` by path, all present as `.so.6.8.2`. The Linux/host branch does `find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Concurrent)` (`CMakeLists.txt:133`) with no minimum version |
| `HOST_MOC` in `$SDK/local/qt6/libexec` (`CMakeLists.txt:100`) | `moc 6.8.2`, runs | Match |
| `HOST_RCC` from the host system (`CMakeLists.txt:104-106`) | SDK rcc broken (needs ICU 76) as InkFeed documents. **This machine has no `rcc` at all** (no `/usr/lib/qt6/libexec/rcc`; cmake and ninja are present), so `find_program(HOST_RCC ... REQUIRED)` would fail here until Qt6 base dev tools (Ubuntu `qt6-base-dev-tools`) are installed | Mismatch with this host |
| Libs linked: `inkview freetype z curl xml2` + `pthread` (`CMakeLists.txt:121`), headers `usr/include/libxml2` (`:125`) | All present (libinkview in `usr/local/lib`, handled by `link_directories(${SYSROOT}/usr/local/lib)` at `:111`) | Match |
| `#include <inkview.h>` in `src/utils/http.cpp:12`, `src/main_qml.cpp:23`, `src/qml_bridge.cpp:30`, `src/core/feed_manager.cpp:7`, `src/core/i18n.cpp:10`; `CloseApp()` (`src/qml_bridge.cpp:493`) | Header at `usr/local/include/inkview.h`; sysroot include path covers `usr/local/include` only if added: InkFeed relies on the compiler default search of the sysroot. `usr/local/include` is a standard GCC include dir, so it resolves. A compile of InkFeed itself was **not** run **[UNVERIFIED]** | Likely OK |

Conclusion: the SDK matches what InkFeed's ARM build expects (compiler prefix, sysroot layout, Qt 6.8.x at `usr/local/qt6`, libinkview, curl, freetype, libxml2, zlib). Only the path assumptions below and the missing host `rcc` need attention. The earlier `build_arm` was evidently produced in a container where the SDK was at `/opt/pocketbook-sdk/sdk_extracted/...` and the source at `/src` (`CMAKE_HOME_DIRECTORY=/src`, CMake 3.28.3, Ninja); `/opt/pocketbook-sdk` does not exist on this machine.

### 7.2 Every SDK path reference in InkFeed

Real SDK layout: `$POCKETBOOK_SDK` (no `sdk_extracted/` level).

| File:line | Current | Problem | Suggested replacement |
|---|---|---|---|
| `CMakeLists.txt:11` | default `SDK_PATH` = `${CMAKE_SOURCE_DIR}/../pocketbook-sdk-6.11/sdk_extracted/arm-buildroot-linux-gnueabihf_sdk-buildroot` | Extra `sdk_extracted/` level does not exist; the default resolves to `<SDK parent directory>/pocketbook-sdk-6.11/sdk_extracted/...` which is absent, so `find_program`/compiler setup fails unless `-DSDK_PATH` is given | `"${CMAKE_SOURCE_DIR}/../pocketbook-sdk-6.11/arm-buildroot-linux-gnueabihf_sdk-buildroot"` |
| `CMakeLists.txt:14,19,20,97,100,111,125` | derived from `SDK_PATH` | no change needed once `:11` is fixed | - |
| `CMakeLists.txt:93,99-103` | comments ("Qt 6.8", SDK moc/rcc) | accurate (SDK Qt is 6.8.2) | - |
| `README.md:57-66` | prose: "SDK 6.11", default `../pocketbook-sdk-6.11/…/arm-buildroot-linux-gnueabihf_sdk-buildroot`, `-DSDK_PATH=…` | The `…` hides the wrong level; text is correct in spirit | Keep, optionally spell out `../pocketbook-sdk-6.11/arm-buildroot-linux-gnueabihf_sdk-buildroot`; note host `qt6-base-dev-tools` is required for `rcc`; note `cmake` and `ninja` can be taken from `$SDK/bin` |
| `README.md:70` | `cmake -S . -B build_arm -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release   # add -DSDK_PATH=… if needed` | ok once default is fixed | - |
| `README.md:77` | `POCKETBOOK_SDK_URL` repository variable | CI only | - |
| `docs/inkpad-one-dev-notes.md:22-27` | "Build against the PocketBook armhf SDK sysroot", host `moc` works, `rcc`/`lrelease` do not | No concrete SDK path. Facts confirmed (moc works, rcc fails on ICU 76) | none; could add the concrete default path |
| `docs/inkpad-one-dev-notes.md:15` | device runs "Qt 6.x" | SDK Qt is 6.8.2; device Qt version itself **[UNVERIFIED]** | optional: state 6.8.2 is what the SDK builds against |
| `.github/workflows/inkpad-one.yml:5-8` | comment about `POCKETBOOK_SDK_URL` tarball of `arm-buildroot-linux-gnueabihf_sdk-buildroot` | fine | - |
| `.github/workflows/inkpad-one.yml:20,29-34,36-47` | `vars.POCKETBOOK_SDK_URL`, cache `~/pbsdk`, key `pbsdk-6.11-rk3566`, 7z extract | no local-path dependency | - |
| `.github/workflows/inkpad-one.yml:49-55` | `find "$HOME/pbsdk" -maxdepth 5 -type d -name '*_sdk-buildroot'` then `SDK_PATH=$root` | depth-search, tolerant of either layout (works with or without `sdk_extracted/`) | - |
| `.github/workflows/inkpad-one.yml:58-60` | `-DSDK_PATH="$SDK_PATH"` | ok | - |
| `.github/workflows/release.yml:56,65-70,72-80,86-90` | same pattern as above | ok | - |
| `build_arm/CMakeCache.txt:18,21,29,33,52,56,101,127,130,133,160,163,213,228,234` | absolute `/opt/pocketbook-sdk/sdk_extracted/arm-buildroot-linux-gnueabihf_sdk-buildroot/...` (CMAKE_ADDR2LINE, AR, *_COMPILER_AR/RANLIB, LINKER, NM, OBJCOPY, OBJDUMP, RANLIB, READELF, STRIP, HOST_MOC, SDK_PATH); also `CMAKE_HOME_DIRECTORY=/src`, `HOST_RCC=/usr/lib/qt6/libexec/rcc` | Stale: the cache was generated in a container. `/opt/pocketbook-sdk` and (on this host) `/usr/lib/qt6/libexec/rcc` do not exist. 25 more lines in `build_arm/build.ninja` and the files under `build_arm/CMakeFiles/` (e.g. `3.28.3/CMake{C,CXX}Compiler.cmake`, `rules.ninja`, `CMakeConfigureLog.yaml`) embed the same prefix | Delete and re-create `build_arm/` (it is gitignored: `git check-ignore build_arm` matches), then `cmake -S . -B build_arm -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release -DSDK_PATH=$POCKETBOOK_SDK`. Do not edit the cache in place |
| `scripts/package-inkpad-one.sh`, `docs/install.md`, `platform/pocketbook/inkfeed.app` | no SDK path | checked via grep | - |

No other SDK path references were found in InkFeed (`grep -rnI -i 'sdk|buildroot|sysroot|libinkview|/opt/'` excluding `.git` and `build_arm`; the only other hits were Windows SDK comments in `lib/sqlite3.c` and an unrelated comment in `include/i18n.h:16`).

Minimal fix suggestion: change `CMakeLists.txt:11` to drop `sdk_extracted/`; keep `-DSDK_PATH` for everything else. Building locally additionally requires a host Qt `rcc` (install `qt6-base-dev-tools`, or equivalent, so `/usr/lib/qt6/libexec/rcc` exists) and a host `cmake`/`ninja` (the SDK's `$SDK/bin/cmake` 3.31.8 and `$SDK/bin/ninja` work as alternatives, unverified in an actual InkFeed configure run).

## 8. Open points

- "6.11" cannot be confirmed from inside the SDK; only the folder name and InkFeed docs state it.
- No actual InkFeed configure/build was run against this SDK (it would write to build directories); compatibility is based on matching paths, versions, and presence of files.
- Whether `uic`, `lrelease`, `qmlcachegen` host tools run was not tested (only `moc` and `rcc` were).
- The device's own Qt version (runtime) was not determined; the SDK's Qt 6.8.2 is the build-time version.
