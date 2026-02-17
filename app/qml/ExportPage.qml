import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root
    signal exportRequested()
    signal backRequested()

    background: Rectangle { color: "#0f1629" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Export"
                color: "#d7e4ff"
                font.pixelSize: 24
                font.bold: true
                Layout.fillWidth: true
            }
            Button { text: "Back"; onClicked: root.backRequested() }
        }

        Text {
            text: "Generate a signed download URL for final video."
            color: "#a9c0ea"
        }

        Button {
            text: "Create Export"
            onClicked: root.exportRequested()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#15213b"
            border.color: "#2e4268"

            TextArea {
                anchors.fill: parent
                anchors.margins: 10
                readOnly: true
                text: "Export status and download URL will be shown here."
                wrapMode: TextArea.Wrap
            }
        }
    }
}
