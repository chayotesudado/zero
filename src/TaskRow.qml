import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Dates.js" as Dates

// One task: the priority-colored circle that completes it, its text, and
// when it's due. Clicking the circle ticks it, then hands it to the backend
// a moment later so the tick is seen.
Item {
    id: row

    property var task: ({})
    property bool selected: false
    property bool showProject: true
    // Shows the grip on hover: the list lets this row be dragged.
    property bool draggable: false
    // Too narrow for every button: Open stays, and the details have the rest.
    property bool narrow: false
    signal completeRequested()
    signal selectRequested()
    signal scheduleRequested()
    signal moveRequested()
    signal deleteRequested()
    signal openRequested()

    readonly property bool completing: tick.running || done
    property bool done: false
    readonly property color priorityColor: Dates.priorityColor(task.priority, theme)
    // In a list grouped by day (Today, Upcoming), the heading says the day,
    // so a task there shows only its time; an overdue one keeps its date.
    property bool groupedByDay: false
    readonly property string due: (translator.language,
        groupedByDay && task.group !== "overdue" ? Dates.timeLabel(task, Qt.locale()) : Dates.dueLabel(task, Qt.locale()))
    readonly property string dueText: task.recurring ? (due !== "" ? "↻ " + due : "↻") : due

    implicitHeight: Math.max(40, content.implicitHeight + 16)

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        radius: Math.min(6, theme.radius)
        // As Omarchy's menus mark the selected row: a foreground tint and the
        // title in the accent, with no outline.
        color: row.selected ? theme.tint(0.08) : hover.hovered ? theme.tint(0.035) : "transparent"
    }

    HoverHandler { id: hover }

    // The circle, the grip, the buttons, and the project are centered on
    // the row, however many lines its text takes.
    Grip {
        visible: row.draggable && hover.hovered && !row.completing
        x: content.anchors.leftMargin - 11
        anchors.verticalCenter: parent.verticalCenter
    }
    // Selects the row, except on its buttons and circle, which do their own
    // thing: hearing their tap too would take the keyboard back to the
    // list from whatever they open.
    TapHandler {
        onTapped: function(point) {
            if (!row.onControl(point.position)) row.selectRequested()
        }
        onDoubleTapped: function(point) {
            if (!row.onControl(point.position)) row.openRequested()
        }
    }
    function onControl(position) {
        for (const control of [actions, circle]) {
            if (!control.visible) continue
            const p = control.mapFromItem(row, position.x, position.y)
            if (control.contains(p)) return true
        }
        return false
    }

    Timer {
        id: tick
        interval: 280
        onTriggered: { row.done = true; row.completeRequested() }
    }

    function complete() {
        if (!completing)
            tick.start()
    }

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16 + (row.task.depth || 0) * 26
        anchors.rightMargin: 16
        spacing: 12

        // A true circle, so it keeps its round shape whatever the theme's
        // corner radius.
        Rectangle {
            id: circle
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 17
            implicitHeight: 17
            radius: 8.5
            color: row.completing ? row.priorityColor : circleHover.hovered ? Qt.alpha(row.priorityColor, 0.2)
                                  : row.task.priority < 4 ? Qt.alpha(row.priorityColor, 0.1) : "transparent"
            border.width: 2
            border.color: row.priorityColor

            Text {
                anchors.centerIn: parent
                text: "✓"
                color: row.completing ? theme.textOn(row.priorityColor) : row.priorityColor
                opacity: row.completing || circleHover.hovered ? 1 : 0
                font.pixelSize: 10
                font.bold: true
            }

            HoverHandler { id: circleHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: row.complete() }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            opacity: row.completing ? 0.45 : 1
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Label {
                Layout.fillWidth: true
                text: row.task.content || ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: 13
                font.strikeout: row.completing
                color: row.selected ? theme.accent : theme.foreground
            }

            Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: (row.task.description || "").split("\n")[0]
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 12
                color: theme.dimForeground
            }

            RowLayout {
                visible: row.dueText !== "" || (row.task.labels || []).length > 0
                spacing: 12

                Label {
                    visible: row.dueText !== ""
                    text: row.dueText
                    textFormat: Text.PlainText
                    font.pixelSize: 11
                    color: row.task.overdue ? theme.red : row.task.dayOffset === 0 ? theme.accent : theme.dimForeground
                }

                Repeater {
                    model: row.task.labels || []
                    Label {
                        required property string modelData
                        text: "@" + modelData
                        textFormat: Text.PlainText
                        font.pixelSize: 11
                        color: theme.dimForeground
                    }
                }
            }
        }

        // Open, date, move, and delete, shown on hover and on the selected task.
        RowLayout {
            id: actions
            Layout.alignment: Qt.AlignVCenter
            visible: hover.hovered || row.selected
            spacing: 2
            RowAction { text: qsTr("Open"); onClicked: row.openRequested() }
            RowAction { visible: !row.narrow; text: qsTr("Date"); onClicked: row.scheduleRequested() }
            RowAction { visible: !row.narrow; text: qsTr("Move"); onClicked: row.moveRequested() }
            RowAction { visible: !row.narrow; text: qsTr("Delete"); danger: true; onClicked: row.deleteRequested() }
        }

        // The project, on the right, when the list mixes projects.
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            visible: row.showProject && !!row.task.projectName
            spacing: 6
            Label {
                text: row.task.projectName || ""
                textFormat: Text.PlainText
                font.pixelSize: 11
                color: theme.dimForeground
                elide: Text.ElideRight
                Layout.maximumWidth: 160
            }
            Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: row.task.projectColor || "#808080" }
        }
    }

    component RowAction: Label {
        id: action
        property bool danger: false
        signal clicked()
        padding: 4
        leftPadding: 8
        rightPadding: 8
        font.pixelSize: 11
        color: actionHover.hovered ? (danger ? theme.red : theme.accent) : theme.dimForeground
        background: Rectangle {
            radius: Math.min(4, theme.radius)
            color: actionHover.hovered ? theme.tint(0.06) : "transparent"
        }
        HoverHandler { id: actionHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: action.clicked() }
    }
}
