pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Zero

// A section's heading in a project's list. It can be selected like a task:
// N then adds into it, Enter renames it, and Delete deletes it.
Item {
    id: row

    property var section: ({})
    property bool selected: false
    property bool draggable: false
    signal selectRequested()
    signal addRequested()
    signal deleteRequested()
    signal renameRequested()

    implicitHeight: 46

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 8
        radius: Math.min(6, Theme.radius)
        // As Omarchy's menus mark the selected row: a foreground tint and the
        // title in the accent, with no outline.
        color: row.selected ? Theme.tint(0.08) : hover.hovered ? Theme.tint(0.035) : "transparent"
    }

    HoverHandler { id: hover }

    Grip {
        visible: row.draggable && hover.hovered
        x: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
    }
    // Selects the heading, except on its buttons: hearing their tap too
    // would take the keyboard back to the list, out of the add box that
    // Add task just opened.
    TapHandler {
        onTapped: function(point) {
            if (!row.onButtons(point.position)) row.selectRequested()
        }
        onDoubleTapped: function(point) {
            if (!row.onButtons(point.position)) row.renameRequested()
        }
    }
    function onButtons(position) {
        if (!buttons.visible) return false
        return buttons.contains(buttons.mapFromItem(row, position.x, position.y))
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 8
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: row.section.name || ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 14
            font.bold: true
            color: row.selected ? Theme.accent : Theme.foreground
        }
        RowLayout {
            id: buttons
            visible: hover.hovered || row.selected
            spacing: 2
            HeadingAction { text: qsTr("Add task"); onClicked: row.addRequested() }
            HeadingAction { text: qsTr("Rename"); onClicked: row.renameRequested() }
            HeadingAction { text: qsTr("Delete"); danger: true; onClicked: row.deleteRequested() }
        }
    }

    // The underline that marks a heading, as in Todoist.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 1
        color: Theme.lighterBackground
    }

    component HeadingAction: Label {
        id: action
        property bool danger: false
        signal clicked()
        padding: 4
        leftPadding: 8
        rightPadding: 8
        font.pixelSize: 11
        color: actionHover.hovered ? (danger ? Theme.red : Theme.accent) : Theme.dimForeground
        background: Rectangle {
            radius: Math.min(4, Theme.radius)
            color: actionHover.hovered ? Theme.tint(0.06) : "transparent"
        }
        HoverHandler { id: actionHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: action.clicked() }
    }
}
