import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: progressPage
    signal backPressed()

    background: Rectangle { color: "#1a1a2e" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Generation Progress"
                font.pixelSize: 24
                font.bold: true
                color: "#e0e0ff"
                Layout.fillWidth: true
            }

            Button {
                text: "Cancel"
                visible: presenter.busy
                onClicked: presenter.cancelGeneration()

                background: Rectangle {
                    color: hovered ? "#ff4444" : "#cc3333"
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font.pixelSize: 12
                }
            }
        }

        Text {
            text: presenter.statusText || "Waiting..."
            color: "#aaaacc"
            font.pixelSize: 14
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 24
            color: "#16213e"
            radius: 12

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, presenter.progress))
                height: parent.height
                radius: 12
                color: presenter.progress >= 1.0 ? "#22cc66" : "#4a9eff"

                Behavior on width {
                    NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                }
                Behavior on color {
                    ColorAnimation { duration: 300 }
                }
            }

            Text {
                anchors.centerIn: parent
                text: Math.round(presenter.progress * 100) + "%"
                color: "#e0e0ff"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0f0f23"
            radius: 8
            border.color: "#2a2a4e"

            ScrollView {
                anchors.fill: parent
                anchors.margins: 12

                TextArea {
                    id: logArea
                    readOnly: true
                    text: presenter.logText
                    color: "#88ccaa"
                    font.family: "monospace"
                    font.pixelSize: 12
                    wrapMode: TextArea.Wrap
                    background: null

                    onTextChanged: {
                        cursorPosition = text.length
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: "#16213e"
            radius: 8
            visible: presenter.outputPath.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "Output:"
                    color: "#8888aa"
                    font.pixelSize: 13
                }
                Text {
                    text: presenter.outputPath
                    color: "#66ccff"
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            text: "New Story"
            visible: !presenter.busy
            font.pixelSize: 14

            background: Rectangle {
                color: hovered ? "#333366" : "#2a2a4e"
                radius: 8
            }
            contentItem: Text {
                text: parent.text
                color: "#aaaacc"
                font: parent.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: progressPage.backPressed()
        }
    }
}
