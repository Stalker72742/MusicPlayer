
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
        color: "#1A1A1A"
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

                RowLayout{
                    anchors.fill: parent
                    Button {
                        id: button
                        width: 250
                        height: 50
                        transformOrigin: Item.Center
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        onClicked: {
                            stackLayout.currentIndex = 0
                        }

                        Label{
                            text: "Search"
                            anchors.centerIn: parent
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        }
                    }

                    Button {
                        id: button1
                        width: 250
                        height: 50
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        onClicked: {
                            stackLayout.currentIndex = 1
                        }

                        Label{
                            text: "Medialib"
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
                anchors.bottomMargin: 150
                currentIndex: 1

                Item {
                    id: search

                    TextField {
                        id: textField
                        height: 25
                        text: " "
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 15
                        anchors.rightMargin: 15
                        anchors.topMargin: 150
                        placeholderTextColor: "#00000000"
                        placeholderText: qsTr("Text Field")
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
