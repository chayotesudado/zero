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
    signal completeRequested()
    signal selectRequested()
    signal scheduleRequested()
    signal moveRequested()
    signal deleteRequested()

    readonly property bool completing: tick.running || done
    property bool done: false
    readonly property color priorityColor: Dates.priorityColor(task.priority, theme)
    readonly property string due: (translator.language, Dates.dueLabel(task, Qt.locale()))

    implicitHeight: Math.max(40, content.implicitHeight + 16)

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        radius: Math.min(6, theme.radius)
        color: row.selected ? theme.tint(0.07) : hover.hovered ? theme.tint(0.035) : "transparent"
        border.width: row.selected ? 1 : 0
        border.color: Qt.alpha(theme.accent, 0.55)
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: row.selectRequested() }

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
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
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
                color: theme.foreground
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
                visible: row.due !== "" || (row.showProject && !!row.task.projectName) || (row.task.labels || []).length > 0
                spacing: 12

                Label {
                    visible: row.due !== ""
                    text: (row.task.recurring ? "↻ " : "") + row.due
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

        // Date, move, and delete, shown on hover and on the selected task.
        RowLayout {
            Layout.alignment: Qt.AlignTop
            visible: hover.hovered || row.selected
            spacing: 2
            RowAction { text: qsTr("Date"); onClicked: row.scheduleRequested() }
            RowAction { text: qsTr("Move"); onClicked: row.moveRequested() }
            RowAction { text: qsTr("Delete"); danger: true; onClicked: row.deleteRequested() }
        }

        // The project, on the right, when the list mixes projects.
        RowLayout {
            Layout.alignment: Qt.AlignTop
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
