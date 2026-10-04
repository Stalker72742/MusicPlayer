pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import SoundLink 1.0

// Delegate for SettingsModel rows.
Item {
    id: root

    required property string key
    required property string title
    required property string description
    required property int type
    required property var value
    required property bool modified
    required property real minimum
    required property real maximum
    required property real step
    required property string unit
    required property var options

    implicitHeight: Math.max(texts.implicitHeight, control.implicitHeight) + 28

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.hairline
    }

    Column {
        id: texts
        anchors.left: parent.left
        anchors.right: controlArea.left
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.bodySize
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: root.description
            color: Theme.palette.textSecondary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.smallSize
        }
    }

    Row {
        id: controlArea
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        TextButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.modified
            text: "Reset"
            onClicked: SettingsModel.resetToDefault(root.key)
        }

        Loader {
            id: control
            anchors.verticalCenter: parent.verticalCenter
            sourceComponent: {
                switch (root.type) {
                case SettingsModel.Toggle:     return toggleControl;
                case SettingsModel.Slider:     return sliderControl;
                case SettingsModel.Choice:     return choiceControl;
                case SettingsModel.FolderList: return foldersControl;
                case SettingsModel.Folder:     return folderControl;
                case SettingsModel.BrowserBridge: return bridgeControl;
                case SettingsModel.OnlineBackends: return backendsControl;
                }
                return null;
            }
        }
    }

    Component {
        id: toggleControl

        ToggleSwitch {
            checked: root.value === true
            onToggled: (checked) => SettingsModel.setValue(root.key, checked)
        }
    }

    Component {
        id: sliderControl

        Row {
            spacing: 12

            ProgressSlider {
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                value: (root.value - root.minimum) / (root.maximum - root.minimum)
                onMoved: (fraction) => {
                    const raw = root.minimum + fraction * (root.maximum - root.minimum);
                    SettingsModel.setValue(root.key, Math.round(raw / root.step) * root.step);
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                text: root.value + " " + root.unit
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }
        }
    }

    // A few short options as pills; many or long ones (e.g. audio devices) as a dropdown.
    Component {
        id: choiceControl

        Loader {
            readonly property bool compactChoice: root.options.length <= 3
                                                  && root.options.join("").length <= 36

            sourceComponent: compactChoice ? pillsChoice : dropdownChoice
        }
    }

    Component {
        id: pillsChoice

        Row {
            spacing: 4

            Repeater {
                model: root.options

                TabPill {
                    required property string modelData

                    text: modelData
                    selected: root.value === modelData
                    onClicked: SettingsModel.setValue(root.key, modelData)
                }
            }
        }
    }

    Component {
        id: dropdownChoice

        Dropdown {
            width: 300
            options: root.options.map(title => ({ id: title, title: title }))
            currentId: root.value
            onActivated: (id) => SettingsModel.setValue(root.key, id)
        }
    }

    Component {
        id: bridgeControl

        Column {
            spacing: 8

            Row {
                anchors.right: parent.right
                spacing: 8

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8; height: 8
                    radius: 4
                    color: OnlineViewModel.bridgeState === OnlineViewModel.Connected ? "#4CC38A"
                         : OnlineViewModel.bridgeState === OnlineViewModel.PairingRequired ? "#E0A845"
                         : Theme.palette.textMuted
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        switch (OnlineViewModel.bridgeState) {
                        case OnlineViewModel.Connected:       return "Connected · " + OnlineViewModel.browserName;
                        case OnlineViewModel.PairingRequired: return "Pairing code";
                        case OnlineViewModel.Waiting:         return "Waiting for the extension";
                        }
                        return "Cannot listen on 127.0.0.1";
                    }
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                }
            }

            // Typed into the extension's popup.
            Text {
                anchors.right: parent.right
                visible: OnlineViewModel.bridgeState === OnlineViewModel.PairingRequired
                text: OnlineViewModel.pairingCode.slice(0, 3) + " " + OnlineViewModel.pairingCode.slice(3)
                color: Theme.palette.textPrimary
                font.family: "Consolas"
                font.pixelSize: 26
                font.weight: Font.Bold
                font.letterSpacing: 2
            }

            Row {
                anchors.right: parent.right
                spacing: 6

                TextButton {
                    visible: OnlineViewModel.pairedCount > 0
                    text: "Forget paired"
                    onClicked: OnlineViewModel.forgetPairedBrowsers()
                }
                TextButton {
                    text: "Open extension folder"
                    onClicked: OnlineViewModel.openExtensionFolder()
                }
            }
        }
    }

    Component {
        id: backendsControl

        Column {
            spacing: 6

            Repeater {
                model: OnlineViewModel.backends

                Row {
                    id: backendRow

                    required property var modelData

                    anchors.right: parent.right
                    spacing: 8

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 300
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                        text: backendRow.modelData.status.length > 0 ? backendRow.modelData.status : "Ready"
                        color: backendRow.modelData.status.length > 0 ? Theme.palette.textSecondary : Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.smallSize
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 150
                        elide: Text.ElideRight
                        text: backendRow.modelData.name + " · " + backendRow.modelData.roles
                        color: Theme.palette.textPrimary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.smallSize
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }

    Component {
        id: folderControl

        Row {
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 320
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideMiddle
                text: root.value
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }
            TextButton {
                anchors.verticalCenter: parent.verticalCenter
                text: "Change"
                onClicked: singleFolderDialog.open()
            }

            FolderDialog {
                id: singleFolderDialog
                title: root.title
                onAccepted: SettingsModel.setFolder(root.key, selectedFolder)
            }
        }
    }

    Component {
        id: foldersControl

        Column {
            spacing: 6

            Repeater {
                model: root.value

                Row {
                    id: folderRow

                    required property string modelData

                    anchors.right: parent.right
                    spacing: 4

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 320
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideMiddle
                        text: folderRow.modelData
                        color: Theme.palette.textSecondary
                        font.family: Theme.typography.family
                        font.pixelSize: Theme.typography.bodySize
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 26; implicitHeight: 26
                        iconName: "close"
                        iconSize: 12
                        onClicked: SettingsModel.removeFolder(root.key, folderRow.modelData)
                    }
                }
            }

            TextButton {
                anchors.right: parent.right
                text: "Add folder"
                onClicked: folderDialog.open()
            }

            FolderDialog {
                id: folderDialog
                title: "Add music folder"
                onAccepted: SettingsModel.addFolder(root.key, selectedFolder)
            }
        }
    }
}
