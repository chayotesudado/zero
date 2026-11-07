import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Picks the project a task moves to: type to filter, arrows to choose,
// Enter to move.
Popup {
    id: picker

    property var task: null
    readonly property var matches: {
        var query = filter.text.trim().toLowerCase()
        var list = backend.destinations
        var out = []
        for (var i = 0; i < list.length; i++)
            if (query === "" || list[i].name.toLowerCase().indexOf(query) >= 0)
                out.push(list[i])
        return out
    }

    function openFor(t) {
        if (!t) return
        task = t
        filter.text = ""
        list.currentIndex = 0
        open()
        filter.forceActiveFocus()
    }

    function choose(index) {
        var target = matches[index]
        if (task && target) backend.moveTask(task.id, target.id)
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
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: picker.task ? qsTr("Move “%1” to").arg(picker.task.content) : ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: theme.foreground
        }

        TextField {
            id: filter
            Layout.fillWidth: true
            placeholderText: qsTr("Project")
            Material.roundedScale: theme.radius
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onUpPressed: list.decrementCurrentIndex()
            onTextChanged: list.currentIndex = 0
            onAccepted: picker.choose(list.currentIndex)
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 320)
            clip: true
            model: picker.matches
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                required property var modelData
                required property int index
                readonly property bool here: picker.task && picker.task.projectId === modelData.id
                width: ListView.view.width
                height: 36
                radius: Math.min(6, theme.radius)
                color: ListView.isCurrentItem ? theme.tint(0.08) : rowHover.hovered ? theme.tint(0.04) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12 + modelData.depth * 14
                    anchors.rightMargin: 12
                    spacing: 10
                    Rectangle {
                        implicitWidth: 8; implicitHeight: 8; radius: 4
                        color: modelData.inbox ? theme.accent : modelData.color
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: theme.foreground
                        font.pixelSize: 14
                    }
                    Label {
                        visible: here
                        text: qsTr("current")
                        color: theme.muted
                        font.pixelSize: 12
                    }
                }
                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: picker.choose(index) }
            }
        }

        Label {
            visible: picker.matches.length === 0
            text: qsTr("No project matches.")
            color: theme.dimForeground
            font.pixelSize: 13
        }
    }
}
