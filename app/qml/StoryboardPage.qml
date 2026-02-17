import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root
    signal saveRequested()
    signal backRequested()

    background: Rectangle { color: "#0f1629" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Storyboard"
                color: "#d7e4ff"
                font.pixelSize: 24
                font.bold: true
                Layout.fillWidth: true
            }
            Button { text: "Back"; onClicked: root.backRequested() }
            Button { text: "Save"; onClicked: root.saveRequested() }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            model: 4
            delegate: Rectangle {
                width: ListView.view.width
                height: 130
                radius: 6
                color: "#172744"
                border.color: "#314f7f"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Text { text: "Scene " + (index + 1); color: "#b8cff7"; font.bold: true }
                    TextField { Layout.fillWidth: true; placeholderText: "Narration" }
                    TextField { Layout.fillWidth: true; placeholderText: "Prompt" }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField { Layout.fillWidth: true; placeholderText: "Duration(ms)"; text: "3000" }
                        ComboBox { model: ["cut", "fade", "zoom"] }
                    }
                }
            }
        }
    }
}
