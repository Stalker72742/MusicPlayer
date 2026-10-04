pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

// Shows the category from Navigation.settingsCategory; tabs and sidebar items both go through Navigation.
Item {
    id: root

    // The setting search pointed at; flashed for a moment.
    property string flashedKey: ""

    function pointAtHighlighted() {
        const key = Navigation.highlightedSetting;
        if (key.length === 0)
            return;

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
            model: SettingsCategoryFilter {
                id: filter
                category: Navigation.settingsCategory
            }

            delegate: SettingRow {
                id: settingRow
                width: list.width

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
