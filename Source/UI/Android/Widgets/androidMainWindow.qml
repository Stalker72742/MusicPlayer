
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.15

Window  {
    visible: true
    width: 540
    height: 960
    color: "#121212"

    Rectangle {
        id: rectangle1
        color: "#121212"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        anchors.topMargin: 0

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: 0

            Row{
                id: row
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: stackLayout.bottom
                anchors.topMargin: 0
                topPadding: 0
                spacing: 10

                Rectangle {
                        anchors.fill: parent
                        color: "#1A1A1A"
                        z: -1
                }

                RowLayout{
                    anchors.fill: parent

                    Button {
                        id: searchButton
                        width: 250
                        height: 50
                        transformOrigin: Item.Center
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                        background: Rectangle {
                                    color: "transparent"
                        }

                        topPadding: 10
                        bottomPadding: 10
                        leftPadding: 20
                        rightPadding: 20

                        onClicked: {
                            stackLayout.currentIndex = 0
                        }

                        Label{
                            text: "Search"

                            color: stackLayout.currentIndex === 0 ? "#FFFFFF" : "#808080"
                            anchors.centerIn: parent
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        }
                    }

                    Button {
                        id: medialib
                        width: 250
                        height: 50
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                        background: Rectangle {
                                    color: "transparent"
                        }

                        topPadding: 10
                        bottomPadding: 10
                        leftPadding: 20
                        rightPadding: 20

                        onClicked: {
                            stackLayout.currentIndex = 1
                        }

                        Label{
                            text: "Medialib"

                            color: stackLayout.currentIndex === 1 ? "#FFFFFF" : "#808080"
                            anchors.centerIn: parent
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        }
                    }

                    Button {
                        id: settings
                        width: 250
                        height: 50
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                        topPadding: 10
                        bottomPadding: 10
                        leftPadding: 20
                        rightPadding: 20

                        background: Rectangle {
                                    color: "transparent"
                        }

                        onClicked: {

                        }

                        Label{
                            text: "Settings"

                            color: stackLayout.currentIndex === 2 ? "#FFFFFF" : "#808080"
                            anchors.centerIn: parent
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        }
                    }
                }
            }

            StackLayout {
                id: stackLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 0
                anchors.rightMargin: 0
                anchors.topMargin: 0
                anchors.bottomMargin: parent.height * 0.12
                currentIndex: 0

                Item {
                    id: search

                    Text {
                        id: searchMenuText

                        text: qsTr("What do you want to listen to?")
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 18;
                        anchors {
                            top: parent.top
                            left: parent.left
                            right: parent.right
                            topMargin: parent.height * 0.1
                            leftMargin: 15
                            rightMargin: 15
                        }

                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    TextField {
                        id: textField
                        anchors {
                            top: searchMenuText.bottom
                            left: parent.left
                            right: parent.right
                            topMargin: parent.height * 0.04
                            leftMargin: parent.width * 0.053
                            rightMargin: parent.width * 0.053
                        }

                        background: Rectangle {
                                color: "#282828"
                                radius: 24
                                border.color: "#282828"
                                border.width: 1
                        }

                        height: Math.max(40, parent.height * 0.06)
                        placeholderText: qsTr("Search...")
                        placeholderTextColor: "#999999"

                        font.pixelSize: Math.min(parent.width, parent.height) * 0.04
                    }
                }

                Item {
                    id: mediaLibPage
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 16

                        Column {
                            width: parent.width
                            spacing: 16

                            PlaylistWidget {
                                width: parent.width
                                height: 90

                                onClicked: {

                                    stackLayout.currentIndex = 1
                                }
                            }
                        }
                    }
                }

                Item {
                    id: playlistPage
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Item{
                            height: 50
                        }

                        Button{

                            text: "Back"
                        }

                        Item{
                            height: 150
                        }

                        Image{
                            width: 100
                            height: 100

                        }

                        Item{

                        }

                        Text{
                            text: "Library"
                        }

                        Item{

                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }
}
