import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    visible: true
    width: 360
    height: 640
    title: "Главное окно"
    color: "#121212"

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // StackView занимает всё, кроме места под панель
        StackView {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: parent.height - bottomBar.height

            initialItem: page1
        }

        // Нижняя панель кнопок
        Rectangle {
            id: bottomBar
            Layout.fillWidth: true
            height: 60
            color: "#1e1e1e"

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Button {
                    Layout.fillWidth: true
                    text: "Стр. 1"
                    onClicked: stackView.replace(page1)
                }
                Button {
                    Layout.fillWidth: true
                    text: "Стр. 2"
                    onClicked: stackView.replace(page2)
                }
                Button {
                    Layout.fillWidth: true
                    text: "Стр. 3"
                    onClicked: stackView.replace(page3)
                }
            }
        }
    }

    // Страницы
    Component {
        id: page1
        Rectangle {
            color: "#212121"
            anchors.fill: parent
            Label {
                text: "Первая страница"
                color: "white"
                anchors.centerIn: parent
            }
        }
    }

    Component {
        id: page2
        Rectangle {
            color: "#263238"
            anchors.fill: parent
            Label {
                text: "Вторая страница"
                color: "white"
                anchors.centerIn: parent
            }
        }
    }

    Component {
        id: page3
        Rectangle {
            color: "#1b5e20"
            anchors.fill: parent
            Label {
                text: "Третья страница"
                color: "white"
                anchors.centerIn: parent
            }
        }
    }
}
