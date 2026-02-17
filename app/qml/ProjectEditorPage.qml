import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root
    signal storyboardRequested()
    signal generationRequested()
    signal assetLibraryRequested()
    signal exportRequested()

    background: Rectangle { color: "#0f1629" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        Text {
            text: "Project Editor"
            color: "#d7e4ff"
            font.pixelSize: 26
            font.bold: true
        }

        TextArea {
            id: storyText
            Layout.fillWidth: true
            Layout.fillHeight: true
            placeholderText: "Input story text..."
            wrapMode: TextArea.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ComboBox {
                id: styleBox
                Layout.fillWidth: true
                model: ["cinematic", "anime", "realistic", "watercolor"]
            }
            SpinBox {
                id: sceneCount
                from: 1
                to: 20
                value: 4
            }
            Button {
                text: "Edit Storyboard"
                onClicked: root.storyboardRequested()
            }
            Button {
                text: "Start Generate"
                onClicked: root.generationRequested()
            }
            Button {
                text: "Assets"
                onClicked: root.assetLibraryRequested()
            }
            Button {
                text: "Export"
                onClicked: root.exportRequested()
            }
        }
    }
}
