#!/bin/sh
# PocketBook launcher for pbnotes on the InkPad One.
#
# Installed at /mnt/ext1/applications/pbnotes.app, with the ELF binary beside it as
# pbnotes.bin. Runtime data and the log live under /mnt/ext1/system/pbnotes.
# Adapted from the InkFeed launcher (MIT, see NOTICE).
#
# Device notes (InkPad One: RK3566, aarch64 kernel, 32-bit armhf userland):
#   - Qt 6.x ships under /usr/lib with its QPA plugins in /usr/plugins/platforms and the
#     QML modules under /usr/qml. The binary was linked for a /usr/local/qt6 prefix that
#     does not exist here, so point Qt at the real locations below.
#   - Qt Quick needs a GL context: use the EGL QPA (pocketbook2egl).
#   - Content rotation (g-sensor) and devicePixelRatio scaling are handled in the app.

APPDIR=/mnt/ext1/applications
DATADIR=/mnt/ext1/system/pbnotes
mkdir -p "$DATADIR"
exec >"$DATADIR/pbnotes.log" 2>&1

echo "=== pbnotes launch: $(date 2>/dev/null) ==="
export LD_LIBRARY_PATH="/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"
export QT_QPA_PLATFORM_PLUGIN_PATH="/usr/plugins/platforms"
export QT_PLUGIN_PATH="/usr/plugins"
export QT_QPA_PLATFORM="pocketbook2egl"
export QML_IMPORT_PATH="/usr/qml"
export QML2_IMPORT_PATH="/usr/qml"

cd /mnt/ext1
"$APPDIR/pbnotes.bin"
echo "=== pbnotes.bin exit code: $? ==="
