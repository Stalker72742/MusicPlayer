pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SoundLink 1.0

// Shows the category from Navigation.settingsCategory; tabs and sidebar items both go through Navigation.
Item {
    id: root

    // Navigation keeps this with the location while the screen is gone; see WindowsMainWindow.
    function saveState() {
        return { scroll: list.contentY - list.originY };
    }

    Component.onCompleted: {
        const y = Navigation.pageState.scroll || 0;
        if (y > 0)
            Qt.callLater(() => list.contentY = list.originY + Math.min(y, Math.max(0, list.contentHeight - list.height)));
    }

    // The setting search pointed at; flashed for a moment.
    property string flashedKey: ""

    function pointAtHighlighted() {
        const key = Navigation.highlightedSetting;
        if (key.length === 0)
            return;

        // A setting search points at must be visible.
        SettingsModel.setSectionCollapsed(SettingsModel.sectionOf(key), false);
        const row = filter.rowOf(key);
        if (row >= 0)
            list.positionViewAtIndex(row, ListView.Contain);
        root.flashedKey = key;
        flashTimer.restart();
    }

    Connections {
        target: Navigation
        // After the category switch has refiltered the list.
        function onHighlightedSettingChanged() { Qt.callLater(root.pointAtHighlighted); }
    }

    Timer {
        id: flashTimer
        interval: 1600
        onTriggered: root.flashedKey = ""
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        spacing: 16

        ScreenHeader {
            Layout.fillWidth: true
            title: "Settings"
            subtitle: SettingsModel.categoryTitle(Navigation.settingsCategory)
        }

        Row {
            spacing: 4
            Layout.topMargin: 4

            Repeater {
                model: SettingsModel.categories

                TabPill {
                    required property var modelData

                    text: modelData.title
                    selected: Navigation.settingsCategory === modelData.id
                    onClicked: Navigation.openSettings(modelData.id)
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 900
            Layout.topMargin: 8
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: AppScrollBar {}
            model: SettingsCategoryFilter {
                id: filter
                category: Navigation.settingsCategory
            }

            section.property: "section"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                id: sectionHeader
                required property string section
                readonly property bool collapsed: SettingsModel.collapsedSections.includes(section)

                width: list.width - 12
                height: 44

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 6
                    anchors.leftMargin: -12
                    anchors.rightMargin: -12
                    radius: Theme.metrics.radiusMd
                    color: headerMouse.containsMouse ? Theme.palette.hoverOverlay : "transparent"
                }

                AppIcon {
                    id: sectionChevron
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 3
                    width: 14; height: 14
                    name: sectionHeader.collapsed ? "chevron-right" : "chevron-down"
                    color: Theme.palette.textSecondary
                }

                Text {
                    anchors.left: sectionChevron.right
                    anchors.leftMargin: 8
                    anchors.verticalCenter: sectionChevron.verticalCenter
                    text: SettingsModel.sectionTitle(sectionHeader.section)
                    color: Theme.palette.textSecondary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.smallSize
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.6
                }

                MouseArea {
                    id: headerMouse
                    anchors.fill: parent
                    anchors.topMargin: 6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: SettingsModel.setSectionCollapsed(sectionHeader.section, !sectionHeader.collapsed)
                }
            }

            delegate: SettingRow {
                id: settingRow
                required property string section
                readonly property bool collapsed: SettingsModel.collapsedSections.includes(section)

                width: list.width - 12
                height: collapsed ? 0 : implicitHeight
                visible: !collapsed

                Rectangle {
                    z: -1
                    anchors.fill: parent
                    anchors.leftMargin: -12
                    anchors.rightMargin: -12
                    radius: Theme.metrics.radiusMd
                    color: Theme.palette.selectedOverlay
                    opacity: root.flashedKey === settingRow.key ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: Theme.motion.durationSlow } }
                }
            }
        }
    }
}
