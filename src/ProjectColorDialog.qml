import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

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
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: theme.darkBackground; border.color: theme.lighterBackground; radius: theme.radius }

    ColumnLayout {
        spacing: 14

        Label {
            Layout.maximumWidth: swatches.implicitWidth
            text: dialog.project ? qsTr("Color for “%1”").arg(dialog.project.name) : ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: theme.foreground
        }

        ColorSwatches {
            id: swatches
            onPicked: function(name) { if (dialog.project) backend.setProjectColor(dialog.project.id, name) }
            onAccepted: dialog.close()
        }
    }
}
