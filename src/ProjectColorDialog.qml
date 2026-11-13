pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Zero

// Recolors a project; the change is saved to Todoist as soon as a color is
// picked, so this just closes on Enter or Esc.
Popup {
    id: dialog

    property var project: null

    function openFor(p) {
        if (!p) return
        project = p
        swatches.selected = p.colorName || "charcoal"
        open()
        swatches.forceActiveFocus()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    padding: 20
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: Theme.darkBackground; border.color: Theme.lighterBackground; radius: Theme.radius }

    ColumnLayout {
        spacing: 14

        Label {
            Layout.maximumWidth: swatches.implicitWidth
            text: dialog.project ? qsTr("Color for “%1”").arg(dialog.project.name) : ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: Theme.foreground
        }

        ColorSwatches {
            id: swatches
            onPicked: function(name) { if (dialog.project) Backend.setProjectColor(dialog.project.id, name) }
            onAccepted: dialog.close()
        }
    }
}
