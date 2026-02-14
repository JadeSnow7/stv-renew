import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: createPage
    signal generationStarted()

    background: Rectangle { color: "#1a1a2e" }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 560)
        spacing: 20

        // Title
        Text {
            text: "🎬 StoryToVideo"
            font.pixelSize: 32
            font.bold: true
            color: "#e0e0ff"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Text {
            text: "Input a story, generate a video."
            font.pixelSize: 14
            color: "#8888aa"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        // Story input
        Rectangle {
            Layout.fillWidth: true
            height: 160
            color: "#16213e"
            radius: 8
            border.color: storyInput.activeFocus ? "#4a9eff" : "#2a2a4e"
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 12

                TextArea {
                    id: storyInput
                    placeholderText: "Enter your story here (e.g., 'A little cat went on an adventure...')"
                    placeholderTextColor: "#555577"
                    color: "#e0e0ff"
                    font.pixelSize: 14
                    wrapMode: TextArea.Wrap
                    background: null
                }
            }
        }

        // Style selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Style:"
                color: "#8888aa"
                font.pixelSize: 14
            }

            ComboBox {
                id: styleSelector
                Layout.fillWidth: true
                model: ["cinematic", "anime", "realistic", "watercolor", "pixel-art"]
                currentIndex: 0

                background: Rectangle {
                    color: "#16213e"
                    radius: 6
                    border.color: "#2a2a4e"
                }
                contentItem: Text {
                    text: styleSelector.displayText
                    color: "#e0e0ff"
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                }
            }
        }

        // Scene count
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Scenes:"
                color: "#8888aa"
                font.pixelSize: 14
            }

            SpinBox {
                id: sceneCount
                from: 2
                to: 8
                value: 4
                editable: true

                background: Rectangle {
                    color: "#16213e"
                    radius: 6
                    border.color: "#2a2a4e"
                }
                contentItem: TextInput {
                    text: sceneCount.textFromValue(sceneCount.value, sceneCount.locale)
                    color: "#e0e0ff"
                    font.pixelSize: 14
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    readOnly: !sceneCount.editable
                    validator: sceneCount.validator
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
            }
        }

        // Generate button
        Button {
            id: generateBtn
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            text: "🚀  Generate Video"
            enabled: storyInput.text.trim().length > 0 && !presenter.busy
            font.pixelSize: 16
            font.bold: true

            background: Rectangle {
                color: generateBtn.enabled
                    ? (generateBtn.hovered ? "#5a5aff" : "#4a4aef")
                    : "#333355"
                radius: 8

                Behavior on color { ColorAnimation { duration: 150 } }
            }
            contentItem: Text {
                text: generateBtn.text
                color: generateBtn.enabled ? "#ffffff" : "#666688"
                font: generateBtn.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                presenter.startGeneration(
                    storyInput.text,
                    styleSelector.currentText,
                    sceneCount.value
                )
                createPage.generationStarted()
            }
        }
    }
}
