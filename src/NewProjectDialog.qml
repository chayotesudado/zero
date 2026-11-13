pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Zero

// Names a new project; Enter creates it and opens it.
Popup {
    id: dialog

    function openNew() {
        field.text = ""
        swatches.selected = "charcoal"
        open()
        field.forceActiveFocus()
    }

    function create() {
        if (field.text.trim() === "") return
        Backend.createProject(field.text, swatches.selected)
        close()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(swatches.implicitWidth + 40, parent ? parent.width - 48 : 420)
    padding: 20
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: Theme.darkBackground; border.color: Theme.lighterBackground; radius: Theme.radius }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: qsTr("New project")
            font.pixelSize: 16
            font.bold: true
            color: Theme.foreground
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Name")
            onAccepted: dialog.create()
            // Down (or Tab) goes on to the colors.
            Keys.onDownPressed: swatches.forceActiveFocus()
        }

        ColorSwatches {
            id: swatches
            Layout.topMargin: 4
            onAccepted: dialog.create()
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Cancel"); flat: true; onClicked: dialog.close() }
            Button {
                id: primaryButton
                text: qsTr("Create")
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
