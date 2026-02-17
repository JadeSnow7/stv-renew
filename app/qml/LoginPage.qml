import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Page {
    id: root
    signal loginSucceeded()

    background: Rectangle { color: "#10182a" }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 420)
        spacing: 14

        Text {
            text: "StoryToVideo Login"
            color: "#d7e4ff"
            font.pixelSize: 30
            font.bold: true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        TextField {
            id: emailInput
            Layout.fillWidth: true
            placeholderText: "email"
            text: "demo@stv.local"
        }

        TextField {
            id: pwdInput
            Layout.fillWidth: true
            placeholderText: "password"
            echoMode: TextInput.Password
            text: "demo123456"
        }

        Button {
            Layout.fillWidth: true
            text: "Login"
            onClicked: root.loginSucceeded()
        }
    }
}
