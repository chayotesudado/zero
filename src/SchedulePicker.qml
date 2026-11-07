import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Changes a task's due date: a quick pick, or anything Todoist understands
// typed in ("fri 9am", "every other monday", "no date").
Popup {
    id: picker

    property var task: null

    // The words stay English: they're what Todoist's date parser reads.
    readonly property var quickPicks: [
        [qsTr("Today"), "today"],
        [qsTr("Tomorrow"), "tomorrow"],
        [qsTr("Next week"), "next monday"],
        [qsTr("No date"), "no date"]
    ]

    function openFor(t) {
        if (!t) return
        task = t
        field.text = ""
        open()
        field.forceActiveFocus()
    }

    function apply(dueString) {
        if (task && dueString.trim() !== "") backend.schedule(task.id, dueString)
        close()
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
            text: picker.task ? qsTr("When is “%1” due?").arg(picker.task.content) : ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: theme.foreground
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Like “fri 5pm” or “every monday”")
            Material.roundedScale: theme.radius
            onAccepted: picker.apply(text)
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: picker.quickPicks
                Button {
                    required property var modelData
                    text: modelData[0]
                    flat: true
                    Material.roundedScale: theme.radius
                    onClicked: picker.apply(modelData[1])
                }
            }
        }
    }
}
