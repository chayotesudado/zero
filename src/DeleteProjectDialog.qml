import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Asks before deleting a project, saying what goes with it: Todoist deletes
// its tasks and sub-projects too, and that can't be undone from here.
Popup {
    id: dialog

    property var project: null
    property int taskCount: -2   // -2 while counting, -1 if counting failed
    property var subprojects: []

    function openFor(p) {
        if (!p) return
        project = p
        taskCount = -2
        subprojects = []
        open()
        cancelButton.forceActiveFocus()
        backend.inspectProject(p.id)
    }

    Connections {
        target: backend
        function onProjectContents(projectId, count, names) {
            if (dialog.project && projectId === dialog.project.id) {
                dialog.taskCount = count
                dialog.subprojects = names
            }
        }
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(440, parent ? parent.width - 48 : 440)
    padding: 20
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: theme.darkBackground; border.color: theme.lighterBackground; radius: theme.radius }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: dialog.project ? qsTr("Delete “%1”?").arg(dialog.project.name) : ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: theme.foreground
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.softForeground
            font.pixelSize: 14
            text: dialog.taskCount === -2 ? qsTr("Counting its tasks…")
                : dialog.taskCount === -1 ? qsTr("Its tasks will be deleted with it.")
                : dialog.taskCount === 0 ? qsTr("It has no open tasks.")
                : qsTr("%n open tasks will be deleted with it.", "", dialog.taskCount)
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.subprojects.length > 0
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: theme.softForeground
            font.pixelSize: 14
            text: qsTr("So will its sub-projects: %1.").arg(dialog.subprojects.join(", "))
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.red
            font.pixelSize: 13
            text: qsTr("This can't be undone.")
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                id: cancelButton
                text: qsTr("Cancel")
                flat: true
                onClicked: dialog.close()
                Keys.onReturnPressed: dialog.close()
            }
            Button {
                text: qsTr("Delete")
                highlighted: true
                enabled: dialog.taskCount !== -2
                contentItem: Label {
                    text: parent.text
                    color: theme.textOn(theme.red)
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 96
                    implicitHeight: 38
                    radius: Math.min(6, theme.radius)
                    color: theme.red
                    opacity: parent.enabled ? (parent.down ? 0.8 : 1) : 0.35
                }
                Material.roundedScale: theme.radius
                onClicked: { backend.deleteProject(dialog.project.id); dialog.close() }
            }
        }
    }
}
