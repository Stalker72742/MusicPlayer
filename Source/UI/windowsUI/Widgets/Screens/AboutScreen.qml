import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        spacing: 24

        ScreenHeader {
            Layout.fillWidth: true
            title: "About"
            subtitle: AppInfoViewModel.name + " — a music player"
        }

        GridLayout {
            columns: 2
            columnSpacing: 32
            rowSpacing: 10

            InfoLabel { text: "Version" }
            InfoValue { text: AppInfoViewModel.version }

            InfoLabel { text: "Qt" }
            InfoValue { text: AppInfoViewModel.qtVersion }
        }

        Item { Layout.fillHeight: true }
    }

    component InfoLabel: Text {
        color: Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
    }

    component InfoValue: Text {
        color: Theme.palette.textPrimary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        font.weight: Font.DemiBold
    }
}
