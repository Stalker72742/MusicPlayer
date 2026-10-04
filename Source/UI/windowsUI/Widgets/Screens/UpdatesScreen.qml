import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SoundLink 1.0

Item {
    id: root

    readonly property int updateState: AppInfoViewModel.updateState
    readonly property bool installing: updateState === AppInfoViewModel.Installing

    readonly property string statusText: {
        switch (root.updateState) {
        case AppInfoViewModel.Checking:        return "Checking for updates…";
        case AppInfoViewModel.UpToDate:        return "You are on the latest version.";
        case AppInfoViewModel.UpdateAvailable: return "Version " + AppInfoViewModel.latestVersion + " is available.";
        case AppInfoViewModel.CheckFailed:     return "Couldn't check for updates.";
        case AppInfoViewModel.Installing:      return "Installing the update…";
        case AppInfoViewModel.InstallFailed:   return "The update was not installed.";
        }
        return "Updates have not been checked yet.";
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        anchors.bottomMargin: 24
        spacing: 20

        ScreenHeader {
            Layout.fillWidth: true
            title: "Updates"
            subtitle: AppInfoViewModel.name + " " + AppInfoViewModel.version
        }

        // How the install that restarted the app went.
        Rectangle {
            visible: AppInfoViewModel.lastResultText.length > 0
            Layout.fillWidth: true
            Layout.maximumWidth: 900
            implicitHeight: resultText.implicitHeight + 24
            radius: Theme.metrics.radiusMd
            color: Theme.palette.fieldBg
            border.width: 1
            border.color: AppInfoViewModel.lastResultOk ? Theme.palette.fieldBorder : Theme.palette.danger

            Text {
                id: resultText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                wrapMode: Text.WordWrap
                text: AppInfoViewModel.lastResultText
                color: AppInfoViewModel.lastResultOk ? Theme.palette.textPrimary : Theme.palette.danger
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
            }
        }

        Column {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.statusText
                color: root.updateState === AppInfoViewModel.CheckFailed || root.updateState === AppInfoViewModel.InstallFailed
                       ? Theme.palette.danger : Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                visible: AppInfoViewModel.errorText.length > 0
                wrapMode: Text.WordWrap
                text: AppInfoViewModel.errorText
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
            Text {
                text: "Source: " + AppInfoViewModel.sourceText
                      + (AppInfoViewModel.lastCheckedText.length > 0 ? " · last checked " + AppInfoViewModel.lastCheckedText : "")
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
        }

        // The updater works without a window and reports here; the app closes and restarts at the end.
        Column {
            visible: root.installing
            Layout.fillWidth: true
            Layout.maximumWidth: 560
            spacing: 8

            Text {
                width: parent.width
                elide: Text.ElideRight
                text: AppInfoViewModel.installStage
                      + (AppInfoViewModel.installProgress >= 0 ? "  " + Math.round(AppInfoViewModel.installProgress * 100) + "%" : "")
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
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
                    width: AppInfoViewModel.installProgress >= 0 ? parent.width * AppInfoViewModel.installProgress : parent.width

                    Behavior on width { NumberAnimation { duration: Theme.motion.durationFast } }

                    // Pulses while the size is unknown.
                    SequentialAnimation on opacity {
                        running: root.installing && AppInfoViewModel.installProgress < 0
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.15; to: 0.6; duration: 700 }
                        NumberAnimation { from: 0.6; to: 0.15; duration: 700 }
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: AppInfoViewModel.name + " will close to replace its files and start again on its own."
                color: Theme.palette.textTertiary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
        }

        Row {
            spacing: 8

            TextButton {
                visible: root.updateState === AppInfoViewModel.UpdateAvailable || root.updateState === AppInfoViewModel.InstallFailed
                primary: true
                text: root.updateState === AppInfoViewModel.InstallFailed ? "Try again" : "Install " + AppInfoViewModel.latestVersion
                onClicked: AppInfoViewModel.installUpdate()
            }
            TextButton {
                primary: root.updateState !== AppInfoViewModel.UpdateAvailable && root.updateState !== AppInfoViewModel.InstallFailed
                enabled: root.updateState !== AppInfoViewModel.Checking && !root.installing
                text: "Check for updates"
                onClicked: AppInfoViewModel.checkForUpdates()
            }
            TextButton {
                enabled: !root.installing
                text: "Open updater"
                onClicked: AppInfoViewModel.openUpdater()
            }
        }

        // Release notes
        Rectangle {
            visible: AppInfoViewModel.releaseNotes.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 900
            radius: Theme.metrics.radiusLg
            color: Theme.palette.fieldBg
            border.width: 1
            border.color: Theme.palette.fieldBorder

            ScrollView {
                anchors.fill: parent
                anchors.margins: 16
                clip: true

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    textFormat: Text.MarkdownText
                    text: AppInfoViewModel.releaseNotes
                    color: Theme.palette.textPrimary
                    linkColor: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }

        Item {
            visible: AppInfoViewModel.releaseNotes.length === 0
            Layout.fillHeight: true
        }

        // Install a local build: a deployed folder or a package zip
        Column {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Install from a local build"
                color: Theme.palette.textPrimary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.bodySize
                font.weight: Font.DemiBold
            }
            Text {
                text: "A deployed folder (with package.json) or a " + AppInfoViewModel.name + "-<version>-win64.zip package."
                color: Theme.palette.textSecondary
                font.family: Theme.typography.family
                font.pixelSize: Theme.typography.smallSize
            }
            Row {
                spacing: 8

                TextButton {
                    enabled: !root.installing
                    text: "Choose folder…"
                    onClicked: folderDialog.open()
                }
                TextButton {
                    enabled: !root.installing
                    text: "Choose archive…"
                    onClicked: archiveDialog.open()
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a deployed " + AppInfoViewModel.name + " folder"
        onAccepted: AppInfoViewModel.installFromPath(selectedFolder)
    }

    FileDialog {
        id: archiveDialog
        title: "Choose a " + AppInfoViewModel.name + " package"
        nameFilters: [AppInfoViewModel.name + " package (" + AppInfoViewModel.name + "-*.zip)", "Zip archives (*.zip)"]
        onAccepted: AppInfoViewModel.installFromPath(selectedFile)
    }
}
