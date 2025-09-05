
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.15

ApplicationWindow  {
    visible: true
    width: 360
    height: 640
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
                        width: 150
                        height: 35
                        text: "Search"
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        onClicked: {
                            stackLayout.currentIndex = 0
                        }
                    }

                    Button {
                        width: 150
                        height: 35
                        text: "Medialib"
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        background: Rectangle{
                            color: "white"
                        }
                        onClicked: {
                            stackLayout.currentIndex = 1
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
                currentIndex: 0

                Item {
                    id: search

                    TextField {
                        id: textField
                        height: 25
                        text: " "
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 150
                        anchors.rightMargin: 150
                        anchors.topMargin: 150
                        placeholderText: qsTr("Text Field")
                    }
                }

                Item {
                    id: mediaLib

                    Rectangle {
                        id: rectangle2
                        height: 200
                        color: "#ffffff"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 75
                        anchors.rightMargin: 75
                        anchors.topMargin: 150
                    }

                }
            }
        }
    }
}
