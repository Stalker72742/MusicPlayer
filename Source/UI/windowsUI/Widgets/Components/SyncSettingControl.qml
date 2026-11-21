pragma ComponentBehavior: Bound

import QtQuick
import SoundLink 1.0

// The right side of the Sync status rows in Settings (SettingsModel.Sync*), drawn from SyncViewModel.
Item {
    id: root

    required property int kind

    // Why the last action did not start; shown under the row's controls.
    property string actionError: ""

    readonly property color okColor: "#4CC38A"
    readonly property color waitColor: "#E0A845"

    implicitWidth: loader.implicitWidth
    implicitHeight: loader.implicitHeight

    function report(error) {
        root.actionError = error || "";
    }

    // Asks for the code a device shows and pairs with it: a device id of a nearby device, or an address.
    function pairWithTarget(target, name, bShowsCode) {
        codeDialog.hint = bShowsCode ? "Type the 6-digit code " + name + " shows."
                                     : "Show a pairing code on " + name + " first, then type its 6 digits here.";
        codeDialog.ask("Pair with " + name, "", (code) => root.report(SyncViewModel.pairWith(target, code)));
    }

    function pairByAddress(initialAddress) {
        addressDialog.ask("Pair by address", initialAddress || "",
                          (address) => root.pairWithTarget(address.trim(), address.trim(), false));
    }

    function formatRemaining(ms) {
        const seconds = Math.max(0, Math.ceil(ms / 1000));
        const rest = seconds % 60;
        return Math.floor(seconds / 60) + ":" + (rest < 10 ? "0" : "") + rest;
    }

    Loader {
        id: loader
        sourceComponent: {
            switch (root.kind) {
            case SettingsModel.SyncThisDevice: return thisDeviceControl;
            case SettingsModel.SyncFiles:      return filesControl;
            case SettingsModel.SyncPairing:    return pairingControl;
            case SettingsModel.SyncDevices:    return devicesControl;
            case SettingsModel.SyncServer:     return serverControl;
            case SettingsModel.SyncAddresses:  return addressesControl;
            }
            return null;
        }
    }

    TextPromptDialog {
        id: renameDialog
        acceptText: "Rename"
        allowEmpty: true
        placeholder: "Device name"
        hint: "Other devices show this name. Leave it empty to use the computer name."
    }
    TextPromptDialog {
        id: codeDialog
        acceptText: "Pair"
        placeholder: "123 456"
    }
    TextPromptDialog {
        id: addressDialog
        acceptText: "Next"
        placeholder: "192.168.1.20 or example.com:47777"
        hint: "The address of a device with SoundLink: host or host:port. The port is 47777 unless another program took it."
    }
    TextPromptDialog {
        id: manualAddressDialog
        acceptText: "Add"
        placeholder: "203.0.113.7:47777"
        hint: "SoundLink tries it whenever a paired device isn't connected, besides the local network."
    }
    ConfirmDialog { id: unpairDialog }

    component Dot: Rectangle {
        width: 8; height: 8
        radius: 4
    }

    component SmallText: Text {
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.smallSize
    }

    component ErrorLine: Text {
        width: 340
        visible: text.length > 0
        horizontalAlignment: Text.AlignRight
        wrapMode: Text.WordWrap
        color: Theme.palette.danger
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.smallSize
    }

    Component {
        id: thisDeviceControl

        Row {
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, 300)
                elide: Text.ElideRight
                text: SyncViewModel.deviceName
                color: Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }
            TextButton {
                anchors.verticalCenter: parent.verticalCenter
                text: "Rename"
                onClicked: renameDialog.ask("Rename this device", SyncViewModel.deviceName,
                                            (name) => SyncViewModel.setDeviceName(name.trim()))
            }
        }
    }

    Component {
        id: filesControl

        Column {
            spacing: 8

            Text {
                anchors.right: parent.right
                width: 340
                horizontalAlignment: Text.AlignRight
                elide: SyncViewModel.fileSyncAvailable ? Text.ElideMiddle : Text.ElideNone
                wrapMode: SyncViewModel.fileSyncAvailable ? Text.NoWrap : Text.WordWrap
                text: !SyncViewModel.running ? "Sync is off"
                    : SyncViewModel.fileSyncAvailable ? SyncViewModel.placementRoot
                    : "There is no music folder, so files aren't synced: only playlists, tags and YouTube tracks"
                color: SyncViewModel.running && !SyncViewModel.fileSyncAvailable ? root.waitColor : Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }

            Row {
                anchors.right: parent.right
                visible: SyncViewModel.running && SyncViewModel.fileSyncAvailable
                spacing: 6

                TextButton {
                    text: "Rescan"
                    onClicked: SyncViewModel.reindex()
                }
                TextButton {
                    text: "Open folder"
                    onClicked: SyncViewModel.openPlacementFolder()
                }
            }
        }
    }

    Component {
        id: pairingControl

        Column {
            id: pairing
            spacing: 8

            property double now: Date.now()

            Timer {
                interval: 1000
                repeat: true
                running: SyncViewModel.pairingOpen
                triggeredOnStart: true
                onTriggered: pairing.now = Date.now()
            }

            SmallText {
                anchors.right: parent.right
                visible: !SyncViewModel.running
                text: "Turn sync on to pair"
            }

            // Typed on the other device.
            Text {
                anchors.right: parent.right
                visible: SyncViewModel.pairingOpen
                text: SyncViewModel.pairingCode.slice(0, 3) + " " + SyncViewModel.pairingCode.slice(3)
                color: Theme.palette.textPrimary
                font.family: "Consolas"
                font.pixelSize: 26
                font.weight: Font.Bold
                font.letterSpacing: 2
            }
            SmallText {
                anchors.right: parent.right
                visible: SyncViewModel.pairingOpen
                text: "Type it on the other device · works for "
                      + root.formatRemaining(SyncViewModel.pairingExpiresMs - pairing.now)
            }

            Row {
                anchors.right: parent.right
                visible: SyncViewModel.pairingWith
                spacing: 8

                Dot {
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.waitColor
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Pairing with " + SyncViewModel.pairingTarget + "…"
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                }
            }

            Text {
                anchors.right: parent.right
                width: 340
                visible: !SyncViewModel.pairingWith && SyncViewModel.pairMessage.length > 0
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: SyncViewModel.pairMessage
                color: SyncViewModel.pairSucceeded ? root.okColor : Theme.palette.danger
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }

            Row {
                anchors.right: parent.right
                visible: SyncViewModel.running
                spacing: 6

                TextButton {
                    visible: !SyncViewModel.pairingOpen
                    text: "Show code"
                    onClicked: root.report(SyncViewModel.openPairing())
                }
                TextButton {
                    visible: SyncViewModel.pairingOpen
                    text: "Hide code"
                    onClicked: SyncViewModel.closePairing()
                }
                TextButton {
                    visible: !SyncViewModel.pairingWith
                    text: "Pair by address…"
                    onClicked: root.pairByAddress("")
                }
                TextButton {
                    visible: SyncViewModel.pairingWith
                    text: "Cancel"
                    onClicked: SyncViewModel.cancelPairWith()
                }
            }

            ErrorLine {
                anchors.right: parent.right
                text: root.actionError
            }
        }
    }

    Component {
        id: devicesControl

        Column {
            spacing: 10

            SmallText {
                anchors.right: parent.right
                visible: SyncViewModel.devices.length === 0
                text: SyncViewModel.running ? "No devices yet: pair one above" : "Sync is off"
            }

            Repeater {
                model: SyncViewModel.devices

                Row {
                    id: deviceRow

                    required property var modelData
                    readonly property bool canPair: !deviceRow.modelData.paired || deviceRow.modelData.rejected

                    anchors.right: parent.right
                    spacing: 10

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 270
                        spacing: 2

                        Row {
                            anchors.right: parent.right
                            spacing: 8

                            Dot {
                                anchors.verticalCenter: parent.verticalCenter
                                color: deviceRow.modelData.connected ? root.okColor
                                     : deviceRow.modelData.rejected ? Theme.palette.danger
                                     : deviceRow.modelData.connecting || deviceRow.modelData.pairing ? root.waitColor
                                     : Theme.palette.textMuted
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.min(implicitWidth, 250)
                                elide: Text.ElideRight
                                text: deviceRow.modelData.name.length > 0 ? deviceRow.modelData.name : "Unnamed device"
                                color: Theme.palette.textPrimary
                                font.family: Theme.typography.family
                                font.pixelSize: Theme.typography.bodySize
                                font.weight: Font.DemiBold
                            }
                        }
                        SmallText {
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            text: (deviceRow.modelData.platform === 2 ? "Android · " : deviceRow.modelData.platform === 1 ? "Windows · " : "")
                                  + deviceRow.modelData.status
                        }
                    }

                    TextButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: deviceRow.canPair
                        enabled: SyncViewModel.running && !SyncViewModel.pairingWith
                        text: deviceRow.modelData.paired ? "Pair again" : "Pair"
                        onClicked: {
                            if (deviceRow.modelData.nearby)
                                root.pairWithTarget(deviceRow.modelData.id, deviceRow.modelData.name, deviceRow.modelData.pairing);
                            else
                                root.pairByAddress(deviceRow.modelData.address);
                        }
                    }
                    TextButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: deviceRow.modelData.paired
                        text: "Unpair"
                        onClicked: unpairDialog.ask("Unpair " + deviceRow.modelData.name,
                            "Nothing more is synced with this device. Files and playlists that came from it stay. "
                            + "If it is connected, it forgets this PC too.",
                            "Unpair", () => SyncViewModel.unpair(deviceRow.modelData.id))
                    }
                }
            }

            ErrorLine {
                anchors.right: parent.right
                text: root.actionError
            }
        }
    }

    Component {
        id: serverControl

        Column {
            spacing: 4

            Row {
                anchors.right: parent.right
                spacing: 8

                Dot {
                    anchors.verticalCenter: parent.verticalCenter
                    color: {
                        if (!SyncViewModel.running)
                            return Theme.palette.textMuted;
                        if (SyncViewModel.listenPort === 0)
                            return Theme.palette.danger;
                        switch (SyncViewModel.reachability) {
                        case SyncViewModel.Public:
                        case SyncViewModel.Mapped:  return root.okColor;
                        case SyncViewModel.Private: return root.waitColor;
                        }
                        return Theme.palette.textMuted;
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        if (!SyncViewModel.running)
                            return "Off";
                        if (SyncViewModel.listenPort === 0)
                            return "Not listening";
                        switch (SyncViewModel.reachability) {
                        case SyncViewModel.Public:
                        case SyncViewModel.Mapped:  return "Can be the server for devices anywhere";
                        case SyncViewModel.Private: return "Local network only";
                        }
                        return "Checking…";
                    }
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                    font.weight: Font.DemiBold
                }
            }

            SmallText {
                anchors.right: parent.right
                width: 360
                visible: SyncViewModel.running
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: SyncViewModel.reachabilityText
            }
            SmallText {
                anchors.right: parent.right
                width: 360
                visible: SyncViewModel.running && SyncViewModel.listenError.length > 0
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: SyncViewModel.listenError
                color: Theme.palette.danger
            }
            SmallText {
                anchors.right: parent.right
                width: 360
                visible: SyncViewModel.running && SyncViewModel.listenPort > 0
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: "Port " + SyncViewModel.listenPort
                      + (SyncViewModel.localEndpoints.length > 0 ? " · on this network: " + SyncViewModel.localEndpoints.join(", ") : "")
            }
            SmallText {
                anchors.right: parent.right
                width: 360
                visible: SyncViewModel.running && SyncViewModel.upnpText.length > 0
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: SyncViewModel.upnpText
            }
            SmallText {
                anchors.right: parent.right
                width: 360
                visible: SyncViewModel.running && !SyncViewModel.hearingBeacons
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: "Another program holds UDP port " + SyncViewModel.listenPort
                      + ", so nearby devices aren't found by themselves. Pair and connect by address instead."
                color: root.waitColor
            }
        }
    }

    Component {
        id: addressesControl

        Column {
            spacing: 6

            Repeater {
                model: SyncViewModel.manualAddresses

                Row {
                    id: addressRow

                    required property var modelData

                    anchors.right: parent.right
                    spacing: 8

                    Dot {
                        anchors.verticalCenter: parent.verticalCenter
                        color: addressRow.modelData.connected ? root.okColor
                             : addressRow.modelData.connecting ? root.waitColor
                             : addressRow.modelData.error.length > 0 ? Theme.palette.danger
                             : Theme.palette.textMuted
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 300
                        spacing: 2

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideMiddle
                            text: addressRow.modelData.address
                            color: Theme.palette.textPrimary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.bodySize
                        }
                        SmallText {
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            text: {
                                const row = addressRow.modelData;
                                if (!SyncViewModel.running)
                                    return "Sync is off";
                                if (row.connected)
                                    return "Connected · " + row.deviceName;
                                if (row.connecting)
                                    return "Connecting…";
                                if (row.error.length > 0)
                                    return row.error;
                                return row.deviceName.length > 0 ? row.deviceName : "Not tried yet";
                            }
                        }
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 26; implicitHeight: 26
                        iconName: "close"
                        iconSize: 12
                        onClicked: SyncViewModel.removeManualAddress(addressRow.modelData.address)
                    }
                }
            }

            TextButton {
                anchors.right: parent.right
                text: "Add address"
                onClicked: manualAddressDialog.ask("Add an address", "",
                                                   (address) => root.report(SyncViewModel.addManualAddress(address)))
            }

            ErrorLine {
                anchors.right: parent.right
                text: root.actionError
            }
        }
    }
}
