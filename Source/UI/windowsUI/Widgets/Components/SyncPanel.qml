pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// The Sync tab of Downloads: what sync is doing, files on their way here and the conflicts waiting for a choice.
// Conflicts can be checked and resolved together; a choice that does not apply to some of them leaves those open.
Item {
    id: root

    /// Ids of the checked conflicts.
    property var selectedIds: []

    readonly property var conflicts: SyncViewModel.conflicts
    readonly property var transfers: SyncViewModel.transfers
    readonly property int selectedCount: selectedIds.length
    readonly property bool allSelected: conflicts.length > 0 && selectedCount === conflicts.length

    /// The list, for the scroll position kept by the Downloads screen.
    property alias listView: conflictList

    readonly property color okColor: "#4CC38A"

    function isSelected(id) {
        return selectedIds.indexOf(id) >= 0;
    }

    function toggleSelected(id) {
        const ids = selectedIds.slice();
        const at = ids.indexOf(id);
        if (at >= 0)
            ids.splice(at, 1);
        else
            ids.push(id);
        selectedIds = ids;
    }

    function selectAll() {
        selectedIds = conflicts.map(conflict => conflict.id);
    }

    function clearSelection() {
        selectedIds = [];
    }

    // How many of the checked conflicts a choice applies to: the label of a choice that does not apply is empty.
    function applicableCount(labelKey) {
        let count = 0;
        for (const item of conflicts) {
            if (selectedIds.indexOf(item.id) >= 0 && item[labelKey].length > 0)
                ++count;
        }
        return count;
    }

    function resolveSelected(choice) {
        SyncViewModel.resolve(selectedIds, choice);
    }

    Connections {
        target: SyncViewModel

        function onConflictsChanged() {
            const open = SyncViewModel.conflicts.map(conflict => conflict.id);
            const kept = root.selectedIds.filter(id => open.indexOf(id) >= 0);
            if (kept.length !== root.selectedIds.length)
                root.selectedIds = kept;
        }
    }

    component SmallText: Text {
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.smallSize
    }

    component TickBox: Rectangle {
        id: box

        property bool checked: false
        /// Some, not all, are checked: drawn as a dash.
        property bool partial: false

        signal toggled

        width: 16; height: 16
        radius: 4
        color: checked || partial ? Theme.palette.accent : "transparent"
        border.width: checked || partial ? 0 : 1
        border.color: boxHover.hovered ? Theme.palette.textSecondary : Theme.palette.textTertiary

        AppIcon {
            anchors.centerIn: parent
            visible: box.checked
            width: 14; height: 14
            name: "check"
            color: Theme.palette.content
        }
        Rectangle {
            anchors.centerIn: parent
            visible: box.partial && !box.checked
            width: 8; height: 2
            radius: 1
            color: Theme.palette.content
        }

        HoverHandler { id: boxHover; cursorShape: Qt.PointingHandCursor }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            onClicked: box.toggled()
        }
    }

    ListView {
        id: conflictList
        anchors.fill: parent
        clip: true
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        model: root.conflicts
        ScrollBar.vertical: AppScrollBar {}

        header: Column {
            width: conflictList.width - 12
            spacing: 16
            bottomPadding: root.conflicts.length > 0 ? 8 : 0

            // What sync is doing.
            Row {
                width: parent.width
                spacing: 12

                AppIcon {
                    id: syncIcon
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20; height: 20
                    name: "sync"
                    color: SyncViewModel.conflictCount > 0 ? Theme.palette.danger
                         : SyncViewModel.running ? Theme.palette.textPrimary : Theme.palette.textMuted

                    RotationAnimation on rotation {
                        running: SyncViewModel.busy
                        loops: Animation.Infinite
                        from: 0; to: 360
                        duration: 1600
                        onRunningChanged: if (!running) syncIcon.rotation = 0
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - syncIcon.width - statusButtons.width - parent.spacing * 2
                    wrapMode: Text.WordWrap
                    text: SyncViewModel.statusText
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                }
                Row {
                    id: statusButtons
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    TextButton {
                        visible: SyncViewModel.running && SyncViewModel.fileSyncAvailable
                        text: "Open folder"
                        onClicked: SyncViewModel.openPlacementFolder()
                    }
                    TextButton {
                        text: "Sync settings"
                        onClicked: Navigation.openSettings("sync")
                    }
                }
            }

            // Files on their way here.
            Column {
                visible: root.transfers.length > 0
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.transfers

                    Item {
                        id: transfer

                        required property var modelData

                        width: parent.width
                        height: 52

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.right: transferSize.left
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 5

                            Text {
                                width: parent.width
                                elide: Text.ElideMiddle
                                text: transfer.modelData.title
                                color: Theme.palette.textPrimary
                                font.family: Theme.typography.family
                                font.pixelSize: Theme.typography.bodySize
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                width: parent.width
                                height: 4
                                radius: 2
                                color: Theme.palette.sliderTrack
                                clip: true

                                Rectangle {
                                    height: parent.height
                                    radius: 2
                                    color: Theme.palette.sliderFill
                                    width: parent.width * transfer.modelData.progress

                                    Behavior on width { NumberAnimation { duration: Theme.motion.durationFast } }
                                }
                            }
                            SmallText {
                                width: parent.width
                                elide: Text.ElideRight
                                text: "From " + transfer.modelData.peerName
                            }
                        }
                        SmallText {
                            id: transferSize
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: transfer.modelData.sizeText
                        }
                    }
                }
            }

            // Conflicts: a check box for all of them and one choice for the checked ones.
            Column {
                visible: root.conflicts.length > 0
                width: parent.width
                spacing: 10

                Row {
                    spacing: 12

                    TickBox {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: root.allSelected
                        partial: root.selectedCount > 0
                        onToggled: root.allSelected || root.selectedCount > 0 ? root.clearSelection() : root.selectAll()
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.selectedCount > 0
                              ? root.selectedCount + " of " + root.conflicts.length + " selected"
                              : (root.conflicts.length === 1 ? "1 conflict" : root.conflicts.length + " conflicts")
                        color: Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.sectionSize
                        font.weight: Font.DemiBold
                    }
                    TextButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.allSelected
                        text: "Select all"
                        onClicked: root.selectAll()
                    }
                }

                Flow {
                    visible: root.selectedCount > 0
                    width: parent.width
                    spacing: 8

                    TextButton {
                        readonly property int applies: root.applicableCount("mineLabel")

                        enabled: applies > 0
                        text: "This PC's version" + (applies > 0 && applies < root.selectedCount ? " (" + applies + ")" : "")
                        onClicked: root.resolveSelected(SyncViewModel.KeepMine)
                    }
                    TextButton {
                        readonly property int applies: root.applicableCount("theirsLabel")

                        enabled: applies > 0
                        text: "The other device's version" + (applies > 0 && applies < root.selectedCount ? " (" + applies + ")" : "")
                        onClicked: root.resolveSelected(SyncViewModel.KeepTheirs)
                    }
                    TextButton {
                        readonly property int applies: root.applicableCount("bothLabel")

                        visible: applies > 0
                        text: "Keep both" + (applies < root.selectedCount ? " (" + applies + ")" : "")
                        onClicked: root.resolveSelected(SyncViewModel.KeepBoth)
                    }
                    TextButton {
                        text: "Clear selection"
                        onClicked: root.clearSelection()
                    }
                }

                SmallText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: root.selectedCount > 0
                          ? "A choice applies to the checked conflicts it fits; the others stay here."
                          : "These changes wait for your choice: the same file or playlist changed on two devices, or a change from another device that replaces or deletes something here."
                }
            }
        }

        delegate: Rectangle {
            id: conflict

            required property var modelData
            readonly property bool selected: root.isSelected(modelData.id)

            width: conflictList.width - 12
            height: Math.max(conflictContent.implicitHeight, conflictActions.height) + 24
            radius: Theme.metrics.radiusMd
            color: selected ? Theme.palette.selectedOverlay
                 : conflictHover.hovered ? Theme.palette.hoverOverlay : "transparent"

            MouseArea {
                anchors.fill: parent
                onClicked: root.toggleSelected(conflict.modelData.id)
            }

            TickBox {
                id: conflictCheck
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.top: parent.top
                anchors.topMargin: 14
                checked: conflict.selected
                onToggled: root.toggleSelected(conflict.modelData.id)
            }

            Column {
                id: conflictContent
                anchors.left: conflictCheck.right
                anchors.leftMargin: 14
                anchors.right: conflictActions.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    width: parent.width
                    elide: Text.ElideMiddle
                    text: conflict.modelData.title
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                    font.weight: Font.DemiBold
                }
                SmallText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: conflict.modelData.detail
                          + (conflict.modelData.peerName.length > 0 ? " · " + conflict.modelData.peerName : "")
                }
                SmallText {
                    width: parent.width
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    color: Theme.palette.textTertiary
                    text: {
                        const parts = [];
                        if (conflict.modelData.mineText.length > 0)
                            parts.push("This PC: " + conflict.modelData.mineText);
                        if (conflict.modelData.theirsText.length > 0)
                            parts.push((conflict.modelData.peerName.length > 0 ? conflict.modelData.peerName : "Other device")
                                       + ": " + conflict.modelData.theirsText);
                        return parts.join("   ·   ");
                    }
                }
            }

            Flow {
                id: conflictActions
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitButtonsWidth, conflict.width * 0.45)
                spacing: 6
                layoutDirection: Qt.RightToLeft

                readonly property real implicitButtonsWidth: (bothButton.visible ? bothButton.implicitWidth + spacing : 0)
                                                             + (theirsButton.visible ? theirsButton.implicitWidth + spacing : 0)
                                                             + (mineButton.visible ? mineButton.implicitWidth : 0)

                TextButton {
                    id: bothButton
                    visible: conflict.modelData.bothLabel.length > 0
                    text: conflict.modelData.bothLabel
                    onClicked: SyncViewModel.resolve([conflict.modelData.id], SyncViewModel.KeepBoth)
                }
                TextButton {
                    id: theirsButton
                    visible: conflict.modelData.theirsLabel.length > 0
                    text: conflict.modelData.theirsLabel
                    onClicked: SyncViewModel.resolve([conflict.modelData.id], SyncViewModel.KeepTheirs)
                }
                TextButton {
                    id: mineButton
                    visible: conflict.modelData.mineLabel.length > 0
                    text: conflict.modelData.mineLabel
                    onClicked: SyncViewModel.resolve([conflict.modelData.id], SyncViewModel.KeepMine)
                }
            }

            HoverHandler { id: conflictHover }
        }

        footer: Item {
            width: conflictList.width - 12
            height: root.conflicts.length > 0 ? 24
                  : Math.max(240, conflictList.height - (conflictList.headerItem ? conflictList.headerItem.height : 0))

            EmptyState {
                anchors.fill: parent
                visible: root.conflicts.length === 0
                iconName: "sync"
                title: SyncViewModel.running ? "Nothing to resolve" : "Sync is off"
                text: SyncViewModel.running
                      ? "When the same file or playlist changes on two devices at once, or a change from another device needs your choice, it waits here."
                      : "Turn on Sync between devices in Settings › Sync to keep music, playlists, tags and saved YouTube tracks the same on your devices."
                actionText: SyncViewModel.running ? "" : "Open Sync settings"
                onActionClicked: Navigation.openSettings("sync")
            }
        }
    }
}
