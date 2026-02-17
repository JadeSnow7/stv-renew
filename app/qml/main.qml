import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    width: 980
    height: 700
    visible: true
    title: "StoryToVideo Renew"
    color: "#0f1629"

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: loginPage
    }

    Component {
        id: loginPage
        LoginPage {
            onLoginSucceeded: stackView.replace(projectListPage)
        }
    }

    Component {
        id: projectListPage
        ProjectListPage {
            onCreateProjectRequested: stackView.push(projectEditorPage)
            onOpenProjectRequested: stackView.push(projectEditorPage)
        }
    }

    Component {
        id: projectEditorPage
        ProjectEditorPage {
            onStoryboardRequested: stackView.push(storyboardPage)
            onGenerationRequested: stackView.push(progressPage)
            onAssetLibraryRequested: stackView.push(assetLibraryPage)
            onExportRequested: stackView.push(exportPage)
        }
    }

    Component {
        id: storyboardPage
        StoryboardPage {
            onBackRequested: stackView.pop()
            onSaveRequested: stackView.pop()
        }
    }

    Component {
        id: progressPage
        ProgressPage {
            onBackPressed: stackView.pop()
        }
    }

    Component {
        id: assetLibraryPage
        AssetLibraryPage {
            onBackRequested: stackView.pop()
        }
    }

    Component {
        id: exportPage
        ExportPage {
            onBackRequested: stackView.pop()
        }
    }
}
