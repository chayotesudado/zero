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
        ["Shift+N", qsTr("Add a subtask to the selected task")],
        ["Enter", qsTr("Open the selected task, or rename the section")],
        ["J / ↓", qsTr("Next task")],
        ["K / ↑", qsTr("Previous task")],
        ["M", qsTr("Move the selected task to a project")],
        ["D", qsTr("Change the selected task's date")],
        ["T", qsTr("Make the selected task due today")],
        ["Ctrl+1–4", qsTr("Set the selected task's priority")],
        ["Ctrl+]  Tab", qsTr("Indent: make it a subtask of the task above")],
        ["Ctrl+[  Shift+Tab", qsTr("Outdent: out from under its parent")],
        ["Alt+J / Alt+K", qsTr("Move the selected task or section down or up")],
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
        ["Alt+Shift+J / K", qsTr("Move the project down or up")],
        ["F2", qsTr("Rename the project on screen")],
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
        id: box
        anchors.centerIn: parent
        // Two columns of shortcuts when there's room; scrolls when even
        // that doesn't fit.
        readonly property bool wide: parent.width >= 900
        width: Math.min(parent.width - 48, wide ? 900 : 460)
        height: Math.min(parent.height - 32, grid.implicitHeight + 80)
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

        Flickable {
            anchors.top: title.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            contentHeight: grid.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            GridLayout {
                id: grid
                width: parent.width
                columns: box.wide ? 4 : 2
                columnSpacing: 20
                rowSpacing: 8

                Repeater {
                    // Wide, an odd count leaves the last right-hand pair blank.
                    model: (grid.columns === 4 ? Math.ceil(overlay.shortcuts.length / 2) * 2 : overlay.shortcuts.length) * 2
                    Label {
                        required property int index
                        // Wide, the list runs down the left column, then the right.
                        readonly property int half: Math.ceil(overlay.shortcuts.length / 2)
                        readonly property int pair: Math.floor(index / 2)
                        readonly property int which: grid.columns === 4
                            ? (pair % 2 === 0 ? pair / 2 : half + (pair - 1) / 2) : pair
                        readonly property var entry: overlay.shortcuts[which] || ["", ""]
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
}
