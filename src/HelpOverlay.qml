import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, toggled with ?.
Rectangle {
    id: overlay
    color: Qt.rgba(0, 0, 0, theme.light ? 0.35 : 0.7)

    readonly property var shortcuts: [
        ["Space", qsTr("Complete the selected task")],
        ["N", qsTr("Add a task to this list (into the selected section)")],
        ["J / ↓", qsTr("Next task")],
        ["K / ↑", qsTr("Previous task")],
        ["M", qsTr("Move the selected task to a project")],
        ["D", qsTr("Change the selected task's date")],
        ["T", qsTr("Make the selected task due today")],
        ["O", qsTr("Open the selected task in Todoist")],
        ["Delete", qsTr("Delete the selected task or section")],
        ["S", qsTr("New section in this project")],
        ["1", qsTr("Inbox")],
        ["2", qsTr("Today")],
        ["3", qsTr("Upcoming")],
        ["[  ]", qsTr("Previous or next list")],
        ["P", qsTr("New project")],
        ["C", qsTr("Change the project's color")],
        ["Shift+M", qsTr("Move the project under another")],
        ["Ctrl+Delete", qsTr("Delete the project on screen")],
        ["R", qsTr("Refresh")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["Esc", qsTr("Leave the add box")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit")]
    ]

    TapHandler { onTapped: overlay.visible = false }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 460)
        height: grid.implicitHeight + 80
        radius: theme.radius
        color: theme.darkBackground
        border.color: theme.lighterBackground

        Label {
            id: title
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 24
            text: qsTr("Keyboard shortcuts")
            font.pixelSize: 18
            font.bold: true
            color: theme.foreground
        }

        GridLayout {
            id: grid
            anchors.top: title.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 24
            anchors.topMargin: 16
            columns: 2
            columnSpacing: 20
            rowSpacing: 8

            Repeater {
                model: overlay.shortcuts.length * 2
                Label {
                    required property int index
                    readonly property var entry: overlay.shortcuts[Math.floor(index / 2)]
                    text: index % 2 === 0 ? entry[0] : entry[1]
                    color: index % 2 === 0 ? theme.accent : theme.softForeground
                    font.pixelSize: 14
                    font.family: index % 2 === 0 ? "monospace" : ""
                    Layout.fillWidth: index % 2 === 1
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
