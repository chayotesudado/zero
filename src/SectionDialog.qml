import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Names a new section in the list on screen; Enter creates it.
Popup {
    id: dialog

    property string projectName: ""

    function openNew(name) {
        projectName = name
        field.text = ""
        open()
        field.forceActiveFocus()
    }

    function create() {
        if (field.text.trim() === "") return
        backend.createSection(field.text)
        close()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, parent ? parent.width - 48 : 420)
    padding: 20
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: theme.darkBackground; border.color: theme.lighterBackground; radius: theme.radius }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("New section in %1").arg(dialog.projectName)
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: theme.foreground
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Name")
            Material.roundedScale: theme.radius
            onAccepted: dialog.create()
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Cancel"); flat: true; onClicked: dialog.close() }
            Button {
                text: qsTr("Create")
                highlighted: true
                enabled: field.text.trim() !== ""
                Material.roundedScale: theme.radius
                contentItem: Label {
                    text: parent.text
                    color: theme.accentForeground
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 96
                    implicitHeight: 38
                    radius: Math.min(6, theme.radius)
                    color: theme.accent
                    opacity: parent.enabled ? (parent.down ? 0.8 : 1) : 0.35
                }
                onClicked: dialog.create()
            }
        }
    }
}
