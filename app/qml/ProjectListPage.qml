import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root
    signal createProjectRequested()
    signal openProjectRequested(string projectId)

    background: Rectangle { color: "#0f1629" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Projects"
                color: "#d7e4ff"
                font.pixelSize: 26
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                text: "New Project"
                onClicked: root.createProjectRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#131f38"
            border.color: "#223251"

            ListView {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                model: ListModel {
                    ListElement { projectId: "demo-1"; title: "Demo Project"; styleName: "cinematic" }
                }
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 56
                    radius: 6
                    color: "#1b2b4a"
                    border.color: "#334d79"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Text { text: title; color: "#e9f0ff"; Layout.fillWidth: true }
                        Text { text: styleName; color: "#8fb2f2" }
                        Button {
                            text: "Open"
                            onClicked: root.openProjectRequested(projectId)
                        }
                    }
                }
            }
        }
    }
}
