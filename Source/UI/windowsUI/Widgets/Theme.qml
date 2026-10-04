pragma Singleton
import QtQuick

QtObject {
    id: theme

    property string name: "dark"

    readonly property QtObject palette: QtObject {
        readonly property color windowTop:     "#0E0E10"
        readonly property color windowBottom:  "#131316"

        readonly property color titleBar:      "#0E0E10"
        readonly property color sideBar:       "#0F0F11"
        readonly property color content:       "#0E0E10"
        readonly property color bottomBar:     "#0E0E10"

        readonly property color border:        "#1B1B1F"
        readonly property color borderStrong:  "#26262A"
        readonly property color hairline:      "#222222"

        readonly property color textPrimary:   "#ECECEC"
        readonly property color textSecondary: "#7A7A80"
        readonly property color textTertiary:  "#54545A"
        readonly property color textMuted:     "#3F3F44"

        readonly property color hoverOverlay:    Qt.rgba(1, 1, 1, 0.04)
        readonly property color pressedOverlay:  Qt.rgba(1, 1, 1, 0.08)
        readonly property color selectedOverlay: Qt.rgba(1, 1, 1, 0.06)

        readonly property color pillSelected:  "#222226"
        readonly property color fieldBg:       "#16161A"
        readonly property color fieldBorder:   "#26262A"

        readonly property color sliderTrack:   "#2A2A2E"
        readonly property color sliderFill:    "#ECECEC"
        readonly property color sliderHandle:  "#FFFFFF"

        readonly property color accent:        "#FFFFFF"
        readonly property color danger:        "#CC2A2A"
        readonly property color badge:         "#E04545"

        readonly property color iconDefault:   "#9A9AA0"
        readonly property color iconActive:    "#ECECEC"
        readonly property color iconMuted:     "#5A5A60"
    }

    readonly property QtObject typography: QtObject {
        readonly property string family:        "Segoe UI"
        readonly property int    titleSize:     28
        readonly property int    sectionSize:   13
        readonly property int    bodySize:      13
        readonly property int    smallSize:     12
        readonly property int    tinySize:      11
        readonly property int    headerSize:    11
        readonly property int    weightTitle:   Font.Bold
        readonly property int    weightSection: Font.DemiBold
        readonly property int    weightBody:    Font.Normal
    }

    readonly property QtObject metrics: QtObject {
        readonly property int titleBarHeight:   40
        readonly property int bottomBarHeight:  90
        readonly property int sideBarExpanded:  240
        readonly property int sideBarCollapsed: 60
        readonly property int rowHeight:        56
        readonly property int navItemHeight:    34
        readonly property int iconSize:         18
        readonly property int iconSmall:        14
        readonly property int iconLarge:        22

        readonly property int radiusSm: 4
        readonly property int radiusMd: 6
        readonly property int radiusLg: 10
        readonly property int radiusXl: 14
        readonly property int radiusPill: 9999

        readonly property int paddingSm: 8
        readonly property int paddingMd: 12
        readonly property int paddingLg: 20
        readonly property int paddingXl: 28
    }

    readonly property QtObject motion: QtObject {
        readonly property int durationFast:   120
        readonly property int durationNormal: 180
        readonly property int durationSlow:   260
        readonly property int easingType: Easing.OutCubic
    }
}
