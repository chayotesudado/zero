import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Picks where something moves: a task to a project or section, or a
// project under another. Type to filter, arrows to choose, Enter to move.
// Items are {key, kind, name, path, depth, color, inbox}, as the backend's
// destinations and parentChoices give them.
Popup {
    id: picker

    property string title: ""
    property var items: []
    // The item it's in now, marked "current".
    property string currentKey: ""
    signal chosen(var item)

    readonly property var matches: {
        var query = filter.text.trim().toLowerCase()
        var out = []
        for (var i = 0; i < items.length; i++)
            if (query === "" || items[i].path.toLowerCase().indexOf(query) >= 0)
                out.push(items[i])
        return out
    }

    function show(heading, list, current) {
        title = heading
        items = list
        currentKey = current || ""
        filter.text = ""
        listView.currentIndex = 0
        open()
        filter.forceActiveFocus()
    }

    function choose(index) {
        var target = matches[index]
        close()
        if (target) chosen(target)
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
            text: picker.title
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: theme.foreground
        }

        TextField {
            id: filter
            Layout.fillWidth: true
            placeholderText: qsTr("Project or section")
            Material.roundedScale: theme.radius
            Keys.onDownPressed: listView.incrementCurrentIndex()
            Keys.onUpPressed: listView.decrementCurrentIndex()
            onTextChanged: listView.currentIndex = 0
            onAccepted: picker.choose(listView.currentIndex)
        }

        ListView {
            id: listView
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
                readonly property bool here: modelData.key === picker.currentKey
                width: ListView.view.width
                height: 36
                radius: Math.min(6, theme.radius)
                color: ListView.isCurrentItem ? theme.tint(0.08) : rowHover.hovered ? theme.tint(0.04) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12 + modelData.depth * 14
                    anchors.rightMargin: 12
                    spacing: 10
                    // A dot for a project, a dash for a section in it.
                    Item {
                        implicitWidth: 8; implicitHeight: 8
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: modelData.kind === "section" ? 2 : 8
                            radius: modelData.kind === "section" ? 1 : 4
                            color: modelData.kind === "top" ? "transparent"
                                 : modelData.inbox ? theme.accent : modelData.color
                            border.width: modelData.kind === "top" ? 1 : 0
                            border.color: theme.muted
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: modelData.kind === "section" ? theme.softForeground : theme.foreground
                        font.pixelSize: modelData.kind === "section" ? 13 : 14
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
            text: qsTr("Nothing matches.")
            color: theme.dimForeground
            font.pixelSize: 13
        }
    }
}
