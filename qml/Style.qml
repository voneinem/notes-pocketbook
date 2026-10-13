pragma Singleton
import QtQuick

// The look every screen shares (spec 010): the colors, line widths, radii, type sizes and heights of the elements taken
// from the reader's own apps. Pages and components read these; none states a pixel size or color of the look itself.
// Sizes are in panel pixels (1404 x 1872, 300 ppi).
QtObject {
    // ink: the panel is monochrome, state is shown by weight, fill and outline, never by color
    readonly property color ink: "#000000"
    readonly property color paper: "#ffffff"
    readonly property color hairline: "#808080"
    readonly property color pressed: "#d8d8d8"
    readonly property color disabled: "#9a9a9a"
    readonly property real scrim: 0.6          // white over the page behind a dialog: the page reads grey

    // lines and corners
    readonly property int hairlineWidth: 1
    readonly property int ruleWidth: 2
    readonly property int heavyWidth: 4
    readonly property int radiusField: 10
    readonly property int radiusDialog: 18

    // heights
    readonly property int touchMin: 96         // the hit area of every icon control
    readonly property int barHeight: 112       // top bar (below topInset)
    readonly property int bottomBarHeight: 92
    readonly property int filterRowHeight: 100
    readonly property int countRowHeight: 60
    readonly property int rowHeight: 132       // a note row (as in 1.5.0)
    readonly property int tileHeight: 190      // a note card
    readonly property int tileMinWidth: 420    // columns = max(2, floor(width / tileMinWidth))
    readonly property int footerHeight: 120    // a dialog's footer cell
    readonly property int dialogPadding: 32    // inside a dialog box, beside the content

    // The height of the system status bar when the system draws it over the app (decided on the device, R-9).
    readonly property real topInset: 0

    // icons
    readonly property int iconTop: 64
    readonly property int iconTool: 56

    // type
    readonly property int titleSize: 44        // upper-case screen title
    readonly property int rowTitleSize: 40     // bold
    readonly property int bodySize: 32
    readonly property int labelSize: 30
    readonly property int captionSize: 28
    readonly property int footerSize: 38

    // The size of a text pixel size in points at the panel's 300 ppi: 30 px = 7 pt, 38 px = 9 pt, 50 px = 12 pt.
    function ptFor(px) { return Math.round(px * 72 / 300) }
}
