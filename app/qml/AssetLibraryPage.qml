import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root
    signal backRequested()

    background: Rectangle { color: "#0f1629" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Asset Library"
                color: "#d7e4ff"
                font.pixelSize: 24
                font.bold: true
                Layout.fillWidth: true
            }
            Button { text: "Back"; onClicked: root.backRequested() }
        }

        ComboBox {
            id: typeFilter
            Layout.fillWidth: true
            model: ["all", "image", "audio", "video_clip", "final_video", "thumbnail"]
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel {
                ListElement { name: "final.mp4"; kind: "final_video"; status: "ready" }
                ListElement { name: "scene1.png"; kind: "image"; status: "ready" }
            }
            delegate: Rectangle {
                width: ListView.view.width
                height: 52
                radius: 6
                color: "#182747"
                border.color: "#2e4c79"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    Text { text: name; color: "#edf2ff"; Layout.fillWidth: true }
                    Text { text: kind; color: "#8db4ff" }
                    Text { text: status; color: "#78d7a2" }
                }
            }
        }
    }
}
