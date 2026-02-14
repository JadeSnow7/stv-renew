import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    width: 900
    height: 650
    visible: true
    title: "StoryToVideo Renew — M1"

    color: "#1a1a2e"

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: createPage
    }

    Component {
        id: createPage
        CreatePage {
            onGenerationStarted: {
                stackView.push(progressPage)
            }
        }
    }

    Component {
        id: progressPage
        ProgressPage {
            onBackPressed: {
                stackView.pop()
            }
        }
    }
}
