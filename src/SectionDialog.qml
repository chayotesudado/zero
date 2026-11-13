pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Zero

// Names a new section in the list on screen, or renames a section or a
// project; Enter does it.
Popup {
    id: dialog

    // "section" for a new one, "rename-section", or "rename-project".
    property string mode: "section"
    property string projectName: ""
    property string targetId: ""
    property string oldName: ""

    function openNew(name) {
        mode = "section"
        projectName = name
        show("")
    }

    function openRenameSection(section) {
        if (!section) return
        mode = "rename-section"
        targetId = section.id
        show(section.name)
    }

    function openRenameProject(project) {
        if (!project) return
        mode = "rename-project"
        targetId = project.id
        show(project.name)
    }

    function show(text) {
        oldName = text
        field.text = text
        open()
        field.forceActiveFocus()
        field.selectAll()
    }

    function create() {
        if (field.text.trim() === "") return
        if (mode === "rename-section") Backend.renameSection(targetId, field.text)
        else if (mode === "rename-project") Backend.renameProject(targetId, field.text)
        else Backend.createSection(field.text)
        close()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, parent ? parent.width - 48 : 420)
    padding: 20
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: Theme.darkBackground; border.color: Theme.lighterBackground; radius: Theme.radius }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: dialog.mode === "rename-section" ? qsTr("Rename the section “%1”").arg(dialog.oldName)
                : dialog.mode === "rename-project" ? qsTr("Rename “%1”").arg(dialog.oldName)
                : qsTr("New section in %1").arg(dialog.projectName)
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: Theme.foreground
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Name")
            onAccepted: dialog.create()
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Cancel"); flat: true; onClicked: dialog.close() }
            Button {
                id: primaryButton
                text: dialog.mode === "section" ? qsTr("Create") : qsTr("Rename")
                highlighted: true
                enabled: field.text.trim() !== ""
                contentItem: Label {
                    text: primaryButton.text
                    color: Theme.accentForeground
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 96
                    implicitHeight: 38
                    radius: Math.min(6, Theme.radius)
                    color: Theme.accent
                    opacity: primaryButton.enabled ? (primaryButton.down ? 0.8 : 1) : 0.35
                }
                onClicked: dialog.create()
            }
        }
    }
}
