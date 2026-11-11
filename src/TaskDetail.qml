import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "Dates.js" as Dates

// A task opened up: its title and description to edit, its priority, date,
// and place, and its subtasks, which open the same way. Everything is saved
// as it's changed (a field when it's left), so closing never loses work.
Popup {
    id: detail

    property string taskId: ""
    property var task: ({})
    property var subtasks: []
    property var parentTask: ({})
    signal scheduleRequested(var task)
    signal moveRequested(var task)
    signal deleteRequested(var task)

    readonly property color priorityColor: Dates.priorityColor(task.priority || 4, theme)

    function openFor(id) {
        if (!id) return
        if (visible) save()
        taskId = id
        reload(true)
        backend.loadSubtasks(id)
        open()
        // Nothing is typed into until it's clicked, so Esc closes at once.
        body.forceActiveFocus()
    }

    // Takes in the task as the backend has it; fields being typed in keep
    // what's typed.
    function reload(all) {
        var t = backend.taskById(taskId)
        if (!t.id) return false
        task = t
        subtasks = backend.subtasksOf(taskId)
        parentTask = t.parentId ? backend.taskById(t.parentId) : ({})
        if (all || !titleField.activeFocus) titleField.text = t.content
        if (all || !descriptionField.activeFocus) descriptionField.text = t.description
        return true
    }

    function save() {
        if (!task.id) return
        backend.editTask(task.id, { content: titleField.text, description: descriptionField.text })
    }

    function complete() {
        save()
        backend.complete(task.id)
        close()
    }

    Connections {
        target: backend
        function onTasksChanged() { if (detail.visible) Qt.callLater(detail.refreshOrClose) }
        function onSubtasksChanged() { if (detail.visible) Qt.callLater(detail.refreshOrClose) }
    }
    // Completed or deleted (here or elsewhere): there's nothing left to show.
    function refreshOrClose() {
        if (visible && !reload(false)) close()
    }

    onClosed: {
        save()
        backend.forgetSubtasks()
        taskId = ""
        task = ({})
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(620, parent ? parent.width - 48 : 620)
    height: Math.min(implicitHeight, parent ? parent.height - 48 : implicitHeight)
    padding: 22
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: theme.darkBackground; border.color: theme.lighterBackground; radius: theme.radius }

    contentItem: Flickable {
        id: body
        implicitHeight: column.implicitHeight
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: column
            width: body.width
            spacing: 14

            // The parent, for a subtask, opens it.
            Label {
                visible: !!detail.parentTask.id
                Layout.fillWidth: true
                text: "↑ " + (detail.parentTask.content || "")
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 12
                color: parentHover.hovered ? theme.accent : theme.dimForeground
                HoverHandler { id: parentHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: detail.openFor(detail.parentTask.id) }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 10
                    implicitWidth: 19
                    implicitHeight: 19
                    radius: 9.5
                    color: circleHover.hovered ? Qt.alpha(detail.priorityColor, 0.2) : "transparent"
                    border.width: 2
                    border.color: detail.priorityColor
                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        visible: circleHover.hovered
                        color: detail.priorityColor
                        font.pixelSize: 11
                        font.bold: true
                    }
                    HoverHandler { id: circleHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: detail.complete() }
                    ToolTip.visible: circleHover.hovered
                    ToolTip.text: qsTr("Complete")
                    ToolTip.delay: 400
                }

                Basic.TextField {
                    id: titleField
                    Layout.fillWidth: true
                    font.pixelSize: 17
                    font.bold: true
                    color: theme.foreground
                    selectionColor: theme.selection
                    selectedTextColor: theme.foreground
                    placeholderText: qsTr("Task name")
                    placeholderTextColor: theme.muted
                    leftPadding: 8
                    rightPadding: 8
                    background: Rectangle {
                        radius: Math.min(6, theme.radius)
                        color: titleField.activeFocus ? theme.tint(0.04) : "transparent"
                        border.width: 1
                        border.color: titleField.activeFocus ? theme.accent : "transparent"
                    }
                    onEditingFinished: detail.save()
                    // Enter keeps the name and leaves the field.
                    onAccepted: body.forceActiveFocus()
                    // Esc closes the details from any field, saving what was typed.
                    Keys.onEscapePressed: detail.close()
                }
            }

            Basic.TextArea {
                id: descriptionField
                Layout.fillWidth: true
                Layout.leftMargin: 31
                Layout.minimumHeight: 64
                wrapMode: TextEdit.Wrap
                font.pixelSize: 13
                color: theme.softForeground
                selectionColor: theme.selection
                selectedTextColor: theme.foreground
                placeholderText: qsTr("Description")
                placeholderTextColor: theme.muted
                padding: 8
                background: Rectangle {
                    radius: Math.min(6, theme.radius)
                    color: descriptionField.activeFocus ? theme.tint(0.04) : "transparent"
                    border.width: 1
                    border.color: descriptionField.activeFocus ? theme.accent : theme.lighterBackground
                }
                onActiveFocusChanged: if (!activeFocus) detail.save()
                Keys.onEscapePressed: detail.close()
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 31
                columns: 2
                columnSpacing: 16
                rowSpacing: 8

                FieldLabel { text: qsTr("Priority") }
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: 4
                        Label {
                            id: chip
                            required property int index
                            readonly property int level: index + 1
                            readonly property bool chosen: detail.task.priority === level
                            readonly property color tone: Dates.priorityColor(level, theme)
                            text: "P" + level
                            font.pixelSize: 12
                            font.bold: chosen
                            horizontalAlignment: Text.AlignHCenter
                            padding: 4
                            leftPadding: 10
                            rightPadding: 10
                            color: chosen ? theme.textOn(tone) : tone
                            background: Rectangle {
                                radius: Math.min(4, theme.radius)
                                color: chip.chosen ? chip.tone : chipHover.hovered ? Qt.alpha(chip.tone, 0.15) : "transparent"
                                border.width: 1
                                border.color: chip.tone
                            }
                            HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: backend.setPriority(detail.task.id, chip.level) }
                            ToolTip.visible: chipHover.hovered
                            ToolTip.text: qsTr("Ctrl+%1").arg(chip.level)
                            ToolTip.delay: 400
                        }
                    }
                }

                FieldLabel { text: qsTr("Date") }
                RowLayout {
                    spacing: 8
                    Label {
                        readonly property string due: (translator.language, Dates.dueLabel(detail.task, Qt.locale()))
                        text: due !== "" ? (detail.task.recurring ? "↻ " : "") + due : qsTr("No date")
                        textFormat: Text.PlainText
                        font.pixelSize: 13
                        color: detail.task.overdue ? theme.red : due !== "" ? theme.foreground : theme.dimForeground
                    }
                    Action2 { text: qsTr("Change (D)"); onClicked: detail.scheduleRequested(detail.task) }
                }

                FieldLabel { text: qsTr("Project") }
                RowLayout {
                    spacing: 8
                    Label {
                        text: detail.task.path || ""
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        Layout.maximumWidth: 320
                        font.pixelSize: 13
                        color: theme.foreground
                    }
                    Action2 { text: qsTr("Move (M)"); onClicked: detail.moveRequested(detail.task) }
                }
            }

            // Subtasks, each opened by a click, and a box to add more.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 31
                Layout.topMargin: 4
                spacing: 2

                Label {
                    text: qsTr("Subtasks")
                    font.pixelSize: 12
                    font.bold: true
                    color: theme.softForeground
                    bottomPadding: 4
                }

                Repeater {
                    model: detail.subtasks
                    Item {
                        id: sub
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 30
                        readonly property color tone: Dates.priorityColor(modelData.priority, theme)

                        Rectangle {
                            anchors.fill: parent
                            radius: Math.min(6, theme.radius)
                            color: subHover.hovered ? theme.tint(0.04) : "transparent"
                        }
                        HoverHandler { id: subHover; cursorShape: Qt.PointingHandCursor }
                        // Opens the subtask, unless the tap was on its circle,
                        // which completes it: this row hears that tap too.
                        TapHandler {
                            onTapped: function(point) {
                                var p = subDone.mapFromItem(sub, point.position.x, point.position.y)
                                if (!subDone.contains(p)) detail.openFor(sub.modelData.id)
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 10
                            Rectangle {
                                id: subDone
                                implicitWidth: 15
                                implicitHeight: 15
                                radius: 7.5
                                color: subCircle.hovered ? Qt.alpha(sub.tone, 0.2) : "transparent"
                                border.width: 2
                                border.color: sub.tone
                                HoverHandler { id: subCircle; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: backend.complete(sub.modelData.id) }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: sub.modelData.content
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                font.pixelSize: 13
                                color: theme.foreground
                            }
                            Label {
                                readonly property string due: (translator.language, Dates.dueLabel(sub.modelData, Qt.locale()))
                                visible: due !== ""
                                text: due
                                font.pixelSize: 11
                                color: sub.modelData.overdue ? theme.red : theme.dimForeground
                            }
                        }
                    }
                }

                Basic.TextField {
                    id: subtaskField
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    implicitHeight: 32
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 0
                    bottomPadding: 0
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 13
                    color: theme.foreground
                    placeholderText: qsTr("Add a subtask (quick add: dates and p1 work)")
                    placeholderTextColor: theme.muted
                    selectionColor: theme.selection
                    selectedTextColor: theme.foreground
                    background: Rectangle {
                        radius: Math.min(6, theme.radius)
                        color: subtaskField.activeFocus ? theme.tint(0.04) : "transparent"
                        border.width: 1
                        border.color: subtaskField.activeFocus ? theme.accent : theme.lighterBackground
                    }
                    onAccepted: {
                        if (text.trim() === "") return
                        backend.add(text, detail.task.id)
                        text = ""
                    }
                    Keys.onEscapePressed: detail.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                // Lined up with the fields, past the actions' own padding.
                Layout.leftMargin: 23
                spacing: 4
                Action2 { text: qsTr("Open in Todoist"); onClicked: Qt.openUrlExternally(detail.task.url) }
                Action2 { text: qsTr("Delete"); danger: true; onClicked: detail.deleteRequested(detail.task) }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Esc to close")
                    font.pixelSize: 11
                    color: theme.muted
                }
            }
        }
    }

    component FieldLabel: Label {
        font.pixelSize: 12
        color: theme.dimForeground
        Layout.alignment: Qt.AlignVCenter
    }

    component Action2: Label {
        id: action
        property bool danger: false
        signal clicked()
        padding: 4
        leftPadding: 8
        rightPadding: 8
        font.pixelSize: 12
        color: actionHover.hovered ? (danger ? theme.red : theme.accent) : theme.dimForeground
        background: Rectangle {
            radius: Math.min(4, theme.radius)
            color: actionHover.hovered ? theme.tint(0.06) : "transparent"
        }
        HoverHandler { id: actionHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: action.clicked() }
    }
}
