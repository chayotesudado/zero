import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "Dates.js" as Dates

ApplicationWindow {
    id: win
    width: 1000
    height: 700
    minimumWidth: 640
    minimumHeight: 460
    visible: true
    title: backend.signedIn && backend.viewName ? qsTr("%1 — Zero").arg(backend.viewName) : "Zero"
    color: theme.background

    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.background: theme.background
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    // Single-key shortcuts stand down while typing into a field.
    readonly property bool typing: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    // Visible, not opened: a popup counts from the moment it starts to open,
    // since opened waits for its animation, and a click that opened one
    // mustn't hand the keyboard back to the list in the meantime.
    readonly property bool pickerOpen: movePicker.visible || schedulePicker.visible
                                       || newProjectDialog.visible || deleteProjectDialog.visible
                                       || projectColorDialog.visible || sectionDialog.visible || confirmDialog.visible
                                       || taskDetail.visible || projectMenu.visible
    readonly property bool keysLive: backend.signedIn && !typing && !helpVisible && !pickerOpen
    // The task details take a few of the list's keys (D, M, Ctrl+1–4) for
    // the task they show, when nothing is open over them.
    readonly property bool detailKeys: taskDetail.visible && !typing && !movePicker.visible && !schedulePicker.visible
                                       && !confirmDialog.visible
    readonly property bool inProject: backend.view.indexOf("project:") === 0
    readonly property string today: Dates.isoToday()
    // Projects and the Inbox have sections; the dated lists don't.
    readonly property bool sectionsHere: backend.view === "inbox" || backend.view.indexOf("project:") === 0

    // The lists in the order [ and ] walk through them.
    readonly property var views: {
        var list = ["inbox", "today", "upcoming"]
        for (var i = 0; i < backend.projects.length; i++)
            list.push("project:" + backend.projects[i].id)
        return list
    }

    function toastShow(message) {
        toast.message = message
        toastTimer.restart()
    }

    function stepView(delta) {
        var i = views.indexOf(backend.view)
        backend.showView(views[(i + delta + views.length) % views.length])
    }

    function currentProject() {
        var list = backend.projects
        for (var i = 0; i < list.length; i++)
            if ("project:" + list[i].id === backend.view) return list[i]
        return null
    }

    // The selected row: a task, or a section's heading.
    function selectedRow() {
        var rows = backend.rows
        return taskList.currentIndex >= 0 && taskList.currentIndex < rows.length ? rows[taskList.currentIndex] : null
    }

    function selectedTask() {
        var row = selectedRow()
        return row && row.kind === "task" ? row : null
    }

    // The task a key acts on: the one the details show, or the selected one.
    function targetTask() {
        return taskDetail.visible ? taskDetail.task : selectedTask()
    }

    // A click on a row takes the keyboard back to the list, unless the click
    // was on one of its buttons and opened something: the row hears that
    // tap too, and taking focus from the popup would leave Esc nowhere to go.
    function focusList() {
        if (!win.pickerOpen) taskList.forceActiveFocus()
    }

    // Back to the details if they're open under what just closed, else the list.
    function refocus() {
        if (taskDetail.visible) taskDetail.contentItem.forceActiveFocus()
        else taskList.forceActiveFocus()
    }

    // Enter: a task opens; a section's heading is renamed.
    function openSelected() {
        var row = selectedRow()
        if (!row) return
        if (row.kind === "section") sectionDialog.openRenameSection(row)
        else taskDetail.openFor(row.id)
    }

    // Alt+J/K: the selected task or section a place down or up.
    function moveSelectedBy(delta) {
        var row = selectedRow()
        if (!row) return
        if (row.kind === "section") backend.moveSectionBy(row.id, delta)
        else backend.moveTaskBy(row.id, delta)
    }

    // Where a drag over a list would drop, by its y in the list: in front of
    // the row under it, or after that row past its middle; count at the end.
    function dropIndexIn(view, count, y) {
        var cy = y + view.contentY
        var i = view.indexAt(view.width / 2, cy)
        if (i < 0) {
            var first = view.itemAtIndex(0)
            return first && cy < first.y ? 0 : count
        }
        var row = view.itemAtIndex(i)
        return cy - row.y < row.height / 2 ? i : i + 1
    }

    // The line marking a drop at index, in the list's content coordinates.
    function dropLineY(view, count, index) {
        var row = view.itemAtIndex(Math.min(index, count - 1))
        if (!row) return -1
        return index >= count ? row.y + row.height : row.y
    }

    // A dragged task over the sidebar lands in the list under the pointer:
    // the Inbox or a project moves it there, Today makes it due today.
    // Returns {view, projectId, name}, or null off the sidebar.
    function sidebarTargetAt(scenePoint) {
        var fixed = [[inboxItem, win.inboxId()], [todayItem, ""]]
        for (var i = 0; i < fixed.length; i++) {
            var item = fixed[i][0]
            var p = item.mapFromItem(null, scenePoint.x, scenePoint.y)
            if (p.x >= 0 && p.y >= 0 && p.x < item.width && p.y < item.height)
                return { view: item.view, projectId: fixed[i][1], name: item.label }
        }
        var q = projectList.mapFromItem(null, scenePoint.x, scenePoint.y)
        if (q.x < 0 || q.y < 0 || q.x >= projectList.width || q.y >= projectList.height) return null
        var index = projectList.indexAt(q.x, q.y + projectList.contentY)
        if (index < 0) return null
        var project = backend.projects[index]
        return { view: "project:" + project.id, projectId: project.id, name: project.name }
    }

    function inboxId() {
        var list = backend.destinations
        for (var i = 0; i < list.length; i++)
            if (list[i].inbox) return list[i].projectId
        return ""
    }

    function addSubtask() {
        var t = selectedTask()
        if (!t) return
        addField.parentTask = t
        addField.forceActiveFocus()
    }

    function completeSelected() {
        var item = taskList.itemAtIndex(taskList.currentIndex)
        if (item) item.complete()
    }

    // The section a new task goes into: the selected heading's, or the
    // selected task's.
    function addSection() {
        var row = sectionsHere ? selectedRow() : null
        if (!row) return ""
        return row.kind === "section" ? row.id : (row.sectionId || "")
    }

    function sectionName(id) {
        var rows = backend.rows
        for (var i = 0; i < rows.length; i++)
            if (rows[i].kind === "section" && rows[i].id === id) return rows[i].name
        return ""
    }

    function moveTask(task) {
        if (!task) return
        movePicker.target = task
        movePicker.mode = "task"
        movePicker.show(qsTr("Move “%1” to").arg(task.content), backend.destinations,
                        task.sectionId ? "section:" + task.sectionId : "project:" + task.projectId)
    }

    function moveProject(project) {
        if (!project) return
        movePicker.target = project
        movePicker.mode = "project"
        movePicker.show(qsTr("Move “%1” under").arg(project.name), backend.parentChoices(project.id),
                        project.parentId ? "project:" + project.parentId : "top")
    }

    // Undoing an add deletes the task from Todoist for good, so it asks
    // first, like Delete does.
    function undo() {
        var doomed = backend.undoDeletes()
        if (!doomed.id) { backend.undo(); return }
        var subtasks = backend.subtaskCount(doomed.id)
        confirmDialog.pending = { kind: "undo-add", id: doomed.id }
        confirmDialog.ask(qsTr("Undo adding “%1”?").arg(doomed.text),
                          subtasks > 0 ? qsTr("Undoing an add deletes the task from Todoist. %n subtasks will be deleted with it.", "", subtasks)
                                       : qsTr("Undoing an add deletes the task from Todoist."),
                          subtasks === 0)
    }

    function deleteRow(row) {
        if (!row) return
        confirmDialog.pending = row
        if (row.kind === "section") {
            confirmDialog.ask(qsTr("Delete the section “%1”?").arg(row.name),
                              row.count > 0 ? qsTr("%n tasks in it will be deleted with it.", "", row.count)
                                            : qsTr("It has no tasks."), false)
        } else {
            var subtasks = backend.subtaskCount(row.id)
            confirmDialog.ask(qsTr("Delete “%1”?").arg(row.content),
                              subtasks > 0 ? qsTr("%n subtasks will be deleted with it.", "", subtasks) : "", subtasks === 0)
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: !win.pickerOpen && (win.helpVisible || addField.activeFocus)
        onActivated: { if (win.helpVisible) win.helpVisible = false; else { addField.text = ""; addField.parentTask = null; taskList.forceActiveFocus() } } }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.completeSelected() }
    Shortcut { sequences: ["N", "A"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: addField.forceActiveFocus() }
    Shortcut { sequences: ["J", "Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: taskList.moveSelection(1) }
    Shortcut { sequences: ["K", "Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: taskList.moveSelection(-1) }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: win.keysLive
        onActivated: { var t = win.selectedTask(); if (t) Qt.openUrlExternally(t.url) } }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: win.keysLive || win.detailKeys; onActivated: win.moveTask(win.targetTask()) }
    Shortcut { sequence: "Shift+M"; context: Qt.ApplicationShortcut; enabled: win.keysLive && backend.view.indexOf("project:") === 0
        onActivated: win.moveProject(win.currentProject()) }
    Shortcut { sequences: ["Delete", "Backspace"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.deleteRow(win.selectedRow()) }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: sectionDialog.openNew(backend.viewName) }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: win.keysLive || win.detailKeys; onActivated: schedulePicker.openFor(win.targetTask()) }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.keysLive
        onActivated: { var t = win.selectedTask(); if (t) backend.schedule(t.id, "today") } }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: newProjectDialog.openNew() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.keysLive && backend.view.indexOf("project:") === 0
        onActivated: projectColorDialog.openFor(win.currentProject()) }
    Shortcut { sequence: "Ctrl+Delete"; context: Qt.ApplicationShortcut; enabled: win.keysLive && backend.view.indexOf("project:") === 0
        onActivated: deleteProjectDialog.openFor(win.currentProject()) }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.refresh() }
    Shortcut { sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.openSelected() }
    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; enabled: taskDetail.visible
        onActivated: taskDetail.close() }
    Shortcut { sequence: "Shift+N"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.addSubtask() }
    Shortcut { sequences: ["Ctrl+]", "Tab"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: { var t = win.selectedTask(); if (t) backend.indentTask(t.id) } }
    Shortcut { sequences: ["Ctrl+[", "Shift+Tab", "Backtab"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: { var t = win.selectedTask(); if (t) backend.outdentTask(t.id) } }
    Shortcut { sequences: ["Alt+J", "Alt+Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: win.moveSelectedBy(1) }
    Shortcut { sequences: ["Alt+K", "Alt+Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: win.moveSelectedBy(-1) }
    Shortcut { sequences: ["Alt+Shift+J", "Alt+Shift+Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.inProject
        onActivated: backend.moveProjectBy(win.currentProject().id, 1) }
    Shortcut { sequences: ["Alt+Shift+K", "Alt+Shift+Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.inProject
        onActivated: backend.moveProjectBy(win.currentProject().id, -1) }
    Shortcut { sequence: "F2"; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.inProject
        onActivated: sectionDialog.openRenameProject(win.currentProject()) }
    // Priority as users count it: Ctrl+1 is p1. Plain 1–3 pick lists.
    Repeater {
        model: 4
        Item {
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                context: Qt.ApplicationShortcut
                enabled: win.keysLive || (taskDetail.visible && !movePicker.visible && !schedulePicker.visible && !confirmDialog.visible)
                onActivated: { var t = win.targetTask(); if (t) backend.setPriority(t.id, index + 1) }
            }
        }
    }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.showView("inbox") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.showView("today") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.showView("upcoming") }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.stepView(-1) }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.stepView(1) }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: backend.signedIn && !win.typing; onActivated: win.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: backend.signedIn && !win.typing; onActivated: backend.redo() }

    Component.onCompleted: if (backend.signedIn) taskList.forceActiveFocus(); else signIn.focusField()

    // Coming back to the window picks up changes made elsewhere.
    onActiveChanged: if (active && backend.signedIn) backend.refresh()

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        SignInView {
            id: signIn
            anchors.fill: parent
            visible: !backend.signedIn
            // Only the visible screen takes the keyboard, so a hidden field
            // never swallows the single-key shortcuts.
            onVisibleChanged: if (visible) focusField(); else taskList.forceActiveFocus()
        }

        RowLayout {
            anchors.fill: parent
            visible: backend.signedIn
            spacing: 0

            // ------------------------------------------------ sidebar
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 230
                color: theme.darkBackground

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 18
                    anchors.bottomMargin: 12
                    spacing: 2

                    RowLayout {
                        Layout.leftMargin: 20
                        Layout.bottomMargin: 14
                        spacing: 10
                        ZeroMark {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            color: theme.accent
                        }
                        Label {
                            text: "Zero"
                            font.pixelSize: 20
                            font.bold: true
                            color: theme.foreground
                        }
                    }

                    SidebarItem { id: inboxItem; view: "inbox"; label: qsTr("Inbox"); glyph: "inbox"; count: backend.inboxCount; hotkey: "1" }
                    SidebarItem { id: todayItem; view: "today"; label: qsTr("Today"); glyph: "today"; count: backend.todayCount; hotkey: "2" }
                    SidebarItem { view: "upcoming"; label: qsTr("Upcoming"); glyph: "upcoming"; hotkey: "3" }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 14
                        Layout.topMargin: 20
                        Layout.bottomMargin: 6
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Projects")
                            font.pixelSize: 12
                            font.bold: true
                            font.capitalization: Font.AllUppercase
                            color: theme.muted
                        }
                        Label {
                            text: "+"
                            font.pixelSize: 18
                            leftPadding: 6
                            rightPadding: 6
                            color: addProjectHover.hovered ? theme.accent : theme.dimForeground
                            HoverHandler { id: addProjectHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: newProjectDialog.openNew() }
                            ToolTip.visible: addProjectHover.hovered
                            ToolTip.text: qsTr("New project (P)")
                            ToolTip.delay: 400
                        }
                    }

                    ListView {
                        id: projectList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: backend.projects
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: SidebarItem {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            view: "project:" + modelData.id
                            label: modelData.name
                            dotColor: modelData.color
                            indent: modelData.depth
                            project: modelData
                        }

                        // A project being dragged, among its siblings.
                        property var dragProject: null
                        property int dropAt: -1
                        function dragMoved(scenePoint) {
                            var p = mapFromItem(null, scenePoint.x, scenePoint.y)
                            dropAt = win.dropIndexIn(projectList, backend.projects.length, p.y)
                        }
                        function dropDragged() {
                            if (dragProject && dropAt >= 0) backend.dropProject(dragProject.id, dropAt)
                            dragProject = null
                            dropAt = -1
                        }

                        DropLine {
                            visible: projectList.dragProject !== null && projectList.dropAt >= 0
                            y: Math.max(0, (projectList.dropAt, projectList.contentHeight,
                                win.dropLineY(projectList, backend.projects.length, projectList.dropAt)) - 1)
                            x: 20 + (projectList.dragProject ? projectList.dragProject.depth : 0) * 14
                            width: projectList.width - x - 12
                        }
                    }

                    Label {
                        Layout.leftMargin: 20
                        visible: !backend.tokenFromEnvironment
                        text: qsTr("Sign out")
                        font.pixelSize: 12
                        color: signOutHover.hovered ? theme.softForeground : theme.muted
                        HoverHandler { id: signOutHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: signOutDialog.open() }
                    }
                }
            }

            Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: theme.lighterBackground }

            // ------------------------------------------------ list
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 32
                    Layout.rightMargin: 24
                    Layout.topMargin: 22
                    spacing: 12

                    Label {
                        text: backend.viewName
                        textFormat: Text.PlainText
                        font.pixelSize: 20
                        font.bold: true
                        color: theme.foreground
                        elide: Text.ElideRight
                        Layout.maximumWidth: 480
                    }
                    Label {
                        Layout.alignment: Qt.AlignBaseline
                        visible: backend.loaded
                        text: qsTr("%n tasks", "", backend.tasks.length)
                        font.pixelSize: 14
                        color: theme.dimForeground
                    }
                    Item { Layout.fillWidth: true }
                    BusyIndicator {
                        running: backend.busy
                        opacity: running ? 1 : 0
                        implicitWidth: 28
                        implicitHeight: 28
                    }
                    Label {
                        text: qsTr("? for shortcuts")
                        font.pixelSize: 12
                        color: theme.muted
                    }
                }

                // A failed request says why, and stays until dismissed or fixed.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 24
                    Layout.topMargin: 12
                    visible: backend.error !== ""
                    implicitHeight: errorRow.implicitHeight + 20
                    radius: Math.min(6, theme.radius)
                    color: theme.mix(theme.red, theme.background, 0.16)
                    border.color: theme.mix(theme.red, theme.background, 0.45)

                    RowLayout {
                        id: errorRow
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.leftMargin: 14
                        Label {
                            Layout.fillWidth: true
                            text: backend.error
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: theme.red
                            font.pixelSize: 13
                        }
                        Button { text: qsTr("Retry"); flat: true; onClicked: { backend.dismissError(); backend.refresh() } }
                        Button { text: qsTr("Dismiss"); flat: true; onClicked: backend.dismissError() }
                    }
                }

                // A plain field sized like a task row; Material's own reserves
                // room for a floating label and stands much taller.
                Basic.TextField {
                    id: addField
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 14
                    implicitHeight: 34
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 0
                    bottomPadding: 0
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 13
                    color: theme.foreground
                    placeholderTextColor: theme.muted
                    selectionColor: theme.selection
                    selectedTextColor: theme.foreground
                    // Says where a new task lands, since that depends on the list.
                    readonly property string section: (taskList.currentIndex, backend.rows, win.sectionName(win.addSection()))
                    // Set by Shift+N: what's added goes under this task,
                    // until Esc or the box is left.
                    property var parentTask: null
                    onActiveFocusChanged: if (!activeFocus) parentTask = null
                    placeholderText: parentTask ? qsTr("Add a subtask to “%1”").arg(parentTask.content)
                                   : backend.view === "today" ? qsTr("Add to Today (N)")
                                   : backend.view === "upcoming" ? qsTr("Add a task (N)")
                                   : section !== "" ? qsTr("Add to %1 / %2 (N)").arg(backend.viewName).arg(section)
                                   : qsTr("Add to %1 (N)").arg(backend.viewName)
                    background: Rectangle {
                        radius: Math.min(6, theme.radius)
                        color: addField.activeFocus ? theme.tint(0.04) : "transparent"
                        border.width: 1
                        border.color: addField.activeFocus ? theme.accent : theme.lighterBackground
                    }
                    onAccepted: {
                        if (text.trim() === "") return
                        if (parentTask) backend.add(text, "", parentTask.id)
                        else backend.add(text, win.addSection())
                        text = ""
                    }
                }
                Label {
                    Layout.leftMargin: 29
                    Layout.topMargin: 4
                    Layout.bottomMargin: 8
                    text: addField.activeFocus
                          ? (addField.parentTask ? qsTr("Goes under “%1”, wherever it is. Dates and p1–p4 work. Esc to stop.").arg(addField.parentTask.content)
                             : backend.view === "upcoming"
                             ? qsTr("Quick add: “Pay rent friday 9am #Home p1 @errand”. Without a #project it goes to the Inbox.")
                             : qsTr("Quick add: dates, #project, @label and p1–p4 work. A #project or date you type wins."))
                          : " "
                    font.pixelSize: 11
                    color: theme.muted
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.rightMargin: 24
                }

                ListView {
                    id: taskList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    clip: true
                    focus: true
                    keyNavigationEnabled: false
                    boundsBehavior: Flickable.StopAtBounds
                    highlightMoveDuration: 0
                    model: backend.rows
                    currentIndex: -1
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    // The selection follows the task, not the row, across
                    // refreshes. Only the user moves it; a model reset can move
                    // currentIndex on its own, so it's restored after one.
                    property string selectedId: ""
                    function select(i) {
                        var rows = backend.rows
                        if (rows.length === 0) { currentIndex = -1; selectedId = ""; return }
                        currentIndex = Math.max(0, Math.min(i, rows.length - 1))
                        selectedId = rows[currentIndex].key
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                    function moveSelection(delta) { select(currentIndex < 0 ? 0 : currentIndex + delta) }
                    function restoreSelection(previous) {
                        var rows = backend.rows
                        for (var i = 0; i < rows.length; i++)
                            if (rows[i].key === selectedId) { currentIndex = i; return }
                        if (selectedId === "") { currentIndex = -1; return }
                        select(previous)
                    }
                    Connections {
                        target: backend
                        function onTasksChanged() {
                            var previous = taskList.currentIndex
                            Qt.callLater(taskList.restoreSelection, previous)
                        }
                        function onViewChanged() { taskList.selectedId = ""; taskList.currentIndex = -1 }
                    }

                    // A row being dragged (a task or a section's heading), and
                    // where it would land: in front of the row at dropAt.
                    // Only projects and the Inbox keep an order to drag in.
                    // A task can also be dropped on the sidebar (dropTarget),
                    // from any list, the dated ones included.
                    property var dragRow: null
                    property int dropAt: -1
                    property var dropTarget: null
                    property point dragPoint: Qt.point(0, 0)
                    property point dragScene: Qt.point(0, 0)
                    function dragMoved(scenePoint) {
                        dragScene = scenePoint
                        dragPoint = mapFromItem(null, scenePoint.x, scenePoint.y)
                        var target = dragRow && dragRow.kind === "task" ? win.sidebarTargetAt(scenePoint) : null
                        // Its own project isn't somewhere else to go.
                        dropTarget = target && target.view !== "today" && target.projectId === dragRow.projectId ? null : target
                        var inList = dragPoint.x >= 0 && dragPoint.x < width
                        dropAt = !dropTarget && inList && win.sectionsHere
                                 ? win.dropIndexIn(taskList, backend.rows.length, dragPoint.y) : -1
                    }
                    function dropDragged() {
                        var row = dragRow, at = dropAt, target = dropTarget
                        dragRow = null
                        dropAt = -1
                        dropTarget = null
                        if (!row) return
                        if (target) {
                            if (target.view === "today") {
                                if (row.dayOffset !== 0 || !row.hasDue) {
                                    backend.schedule(row.id, "today")
                                    win.toastShow(qsTr("“%1” is due today. Ctrl+Z to undo.").arg(row.content))
                                }
                            } else if (target.projectId && target.projectId !== row.projectId) {
                                backend.moveTask(row.id, target.projectId, "")
                                win.toastShow(qsTr("Moved “%1” to %2. Ctrl+Z to undo.").arg(row.content).arg(target.name))
                            }
                            return
                        }
                        if (at < 0) return
                        if (row.kind === "section") backend.dropSection(row.id, at)
                        else backend.dropTask(row.id, at)
                    }
                    // The level a task would take, to indent the line by: the
                    // task it lands in front of, past itself and its subtasks,
                    // as the backend reads a drop.
                    readonly property int dropDepth: {
                        var rows = backend.rows
                        if (!dragRow || dragRow.kind === "section" || dropAt < 0) return 0
                        var from = -1, to = -1
                        for (var k = 0; k < rows.length; k++)
                            if (rows[k].key === dragRow.key) from = k
                        to = from + 1
                        while (from >= 0 && to < rows.length && rows[to].kind === "task" && rows[to].depth > rows[from].depth) to++
                        for (var i = dropAt; i < rows.length; i++) {
                            if (i >= from && i < to) continue
                            return rows[i].kind === "task" ? (rows[i].depth || 0) : 0
                        }
                        return 0
                    }

                    // Near the top or bottom edge, the list scrolls under the drag.
                    Timer {
                        interval: 16
                        repeat: true
                        running: taskList.dropAt >= 0 && (taskList.dragPoint.y < 36 || taskList.dragPoint.y > taskList.height - 36)
                        onTriggered: {
                            var top = taskList.originY
                            var bottom = taskList.originY + Math.max(0, taskList.contentHeight - taskList.height)
                            var step = taskList.dragPoint.y < 36 ? -8 : 8
                            taskList.contentY = Math.max(top, Math.min(bottom, taskList.contentY + step))
                            taskList.dropAt = win.dropIndexIn(taskList, backend.rows.length, taskList.dragPoint.y)
                        }
                    }

                    // Sections are made from here too, not only with S.
                    footer: Item {
                        width: ListView.view.width
                        height: win.sectionsHere && backend.loaded ? 48 : 0
                        visible: height > 0
                        Label {
                            id: addSectionLabel
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("+ Add section (S)")
                            font.pixelSize: 13
                            padding: 4
                            color: addSectionHover.hovered ? theme.accent : theme.dimForeground
                            HoverHandler { id: addSectionHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: sectionDialog.openNew(backend.viewName) }
                        }
                    }

                    DropLine {
                        visible: taskList.dragRow !== null && taskList.dropAt >= 0
                        y: Math.max(0, (taskList.dropAt, taskList.contentHeight,
                            win.dropLineY(taskList, backend.rows.length, taskList.dropAt)) - 1)
                        x: 16 + taskList.dropDepth * 26
                        width: taskList.width - x - 16
                        z: 5
                    }

                    readonly property bool grouped: backend.view === "today" || backend.view === "upcoming"
                    section.property: grouped ? "group" : ""
                    section.delegate: Item {
                        required property string section
                        width: ListView.view.width
                        height: 34
                        Label {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 20
                            anchors.bottomMargin: 6
                            text: (translator.language, Dates.sectionLabel(parent.section, win.today, Qt.locale()))
                            font.pixelSize: 12
                            font.bold: true
                            color: parent.section === "overdue" ? theme.red : theme.softForeground
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            height: 1
                            color: theme.lighterBackground
                        }
                    }

                    // A task, or a section's heading.
                    delegate: Item {
                        id: entry
                        required property var modelData
                        required property int index
                        readonly property bool isSection: modelData.kind === "section"
                        readonly property bool current: ListView.isCurrentItem
                        width: ListView.view.width
                        height: loader.item ? loader.item.implicitHeight : 0
                        opacity: taskList.dragRow && taskList.dragRow.key === modelData.key ? 0.35 : 1

                        function complete() { if (!isSection && loader.item) loader.item.complete() }

                        // Dragging picks the row up; it lands where the line shows.
                        DragHandler {
                            target: null
                            enabled: win.sectionsHere || !entry.isSection
                            cursorShape: Qt.ClosedHandCursor
                            onActiveChanged: {
                                if (active) {
                                    taskList.select(entry.index)
                                    taskList.forceActiveFocus()
                                    taskList.dragRow = entry.modelData
                                } else {
                                    taskList.dropDragged()
                                }
                            }
                            onCentroidChanged: if (active) taskList.dragMoved(centroid.scenePosition)
                        }
                        // A refresh that rebuilds the list mid-drag ends it.
                        Component.onDestruction: if (taskList.dragRow && taskList.dragRow.key === modelData.key) taskList.dragRow = null

                        Loader {
                            id: loader
                            width: parent.width
                            sourceComponent: entry.isSection ? sectionComponent : taskComponent
                        }
                        Component {
                            id: taskComponent
                            TaskRow {
                                task: entry.modelData
                                selected: entry.current
                                draggable: true
                                showProject: taskList.grouped
                                onSelectRequested: { taskList.select(entry.index); win.focusList() }
                                onCompleteRequested: backend.complete(entry.modelData.id)
                                onScheduleRequested: { taskList.select(entry.index); schedulePicker.openFor(entry.modelData) }
                                onMoveRequested: { taskList.select(entry.index); win.moveTask(entry.modelData) }
                                onDeleteRequested: { taskList.select(entry.index); win.deleteRow(entry.modelData) }
                                onOpenRequested: { taskList.select(entry.index); taskDetail.openFor(entry.modelData.id) }
                            }
                        }
                        Component {
                            id: sectionComponent
                            SectionRow {
                                section: entry.modelData
                                selected: entry.current
                                draggable: true
                                onSelectRequested: { taskList.select(entry.index); win.focusList() }
                                onAddRequested: { taskList.select(entry.index); addField.forceActiveFocus() }
                                onDeleteRequested: { taskList.select(entry.index); win.deleteRow(entry.modelData) }
                                onRenameRequested: { taskList.select(entry.index); sectionDialog.openRenameSection(entry.modelData) }
                            }
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        visible: backend.loaded && backend.rows.length === 0
                        spacing: 8
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "✓"
                            font.pixelSize: 44
                            color: theme.accent
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.view === "today" ? qsTr("Nothing left for today.") : qsTr("Nothing here.")
                            font.pixelSize: 16
                            color: theme.dimForeground
                        }
                    }
                }
            }
        }

        // A short note after adding, since quick add may file the task
        // somewhere other than the list on screen.
        Rectangle {
            id: toast
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            radius: Math.min(8, theme.radius)
            color: theme.lighterBackground
            border.color: theme.muted
            implicitWidth: toastLabel.implicitWidth + 32
            implicitHeight: toastLabel.implicitHeight + 20
            opacity: toastTimer.running ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 160 } }
            property string message: ""
            Label {
                id: toastLabel
                anchors.centerIn: parent
                text: toast.message
                textFormat: Text.PlainText
                color: theme.foreground
                font.pixelSize: 13
            }
            Timer { id: toastTimer; interval: 2600 }
            Connections {
                target: backend
                function onAdded(content) {
                    toast.message = qsTr("Added “%1”. Ctrl+Z to undo.").arg(content)
                    toastTimer.restart()
                }
            }
        }

        // What's being dragged, under the pointer, over the list or the
        // sidebar, saying where it goes when that's another list.
        Rectangle {
            visible: taskList.dragRow !== null
            z: 50
            x: taskList.dragScene.x + 14
            y: taskList.dragScene.y - height / 2
            width: Math.min(ghostLabel.implicitWidth + 24, 340)
            height: ghostLabel.implicitHeight + 12
            radius: Math.min(6, theme.radius)
            color: theme.lighterBackground
            border.color: theme.accent
            opacity: 0.95
            Label {
                id: ghostLabel
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                width: parent.width - 24
                elide: Text.ElideRight
                textFormat: Text.PlainText
                readonly property var row: taskList.dragRow
                readonly property var target: taskList.dropTarget
                text: !row ? "" : (row.kind === "section" ? row.name : row.content)
                      + (target ? (target.view === "today" ? "  →  " + qsTr("due today") : "  →  " + target.name) : "")
                font.pixelSize: 13
                font.bold: !!row && row.kind === "section"
                color: theme.foreground
            }
        }

        MovePicker {
            id: movePicker
            // What's moving: a task, or a project under another.
            property string mode: "task"
            property var target: null
            onClosed: win.refocus()
            onChosen: function(item) {
                if (!target) return
                if (mode === "project") backend.moveProject(target.id, item.projectId)
                else backend.moveTask(target.id, item.projectId, item.sectionId)
            }
        }

        SectionDialog {
            id: sectionDialog
            onClosed: taskList.forceActiveFocus()
        }

        ConfirmDialog {
            id: confirmDialog
            property var pending: null
            onClosed: win.refocus()
            onConfirmed: {
                if (!pending) return
                if (pending.kind === "undo-add") {
                    // Only if that add is still the next undo; something
                    // may have landed on the stack while this was open.
                    if (backend.undoDeletes().id === pending.id) backend.undo()
                } else if (pending.kind === "section") backend.deleteSection(pending.id)
                else backend.deleteTask(pending.id)
            }
        }

        SchedulePicker {
            id: schedulePicker
            onClosed: win.refocus()
        }

        TaskDetail {
            id: taskDetail
            onClosed: taskList.forceActiveFocus()
            onScheduleRequested: function(task) { schedulePicker.openFor(task) }
            onMoveRequested: function(task) { win.moveTask(task) }
            onDeleteRequested: function(task) { win.deleteRow(task) }
        }

        NewProjectDialog {
            id: newProjectDialog
            onClosed: taskList.forceActiveFocus()
        }

        ProjectColorDialog {
            id: projectColorDialog
            onClosed: taskList.forceActiveFocus()
        }

        DeleteProjectDialog {
            id: deleteProjectDialog
            onClosed: taskList.forceActiveFocus()
        }

        Menu {
            id: projectMenu
            property var project: null
            Material.roundedScale: theme.radius
            MenuItem {
                text: qsTr("Rename…")
                onTriggered: sectionDialog.openRenameProject(projectMenu.project)
            }
            MenuItem {
                text: qsTr("Change color…")
                onTriggered: projectColorDialog.openFor(projectMenu.project)
            }
            MenuItem {
                text: qsTr("Move under…")
                onTriggered: win.moveProject(projectMenu.project)
            }
            MenuItem {
                text: qsTr("Move up")
                onTriggered: backend.moveProjectBy(projectMenu.project.id, -1)
            }
            MenuItem {
                text: qsTr("Move down")
                onTriggered: backend.moveProjectBy(projectMenu.project.id, 1)
            }
            MenuItem {
                text: qsTr("Delete project…")
                onTriggered: deleteProjectDialog.openFor(projectMenu.project)
            }
        }

        Connections {
            target: backend
            function onProjectDeleted(name) { win.toastShow(qsTr("Deleted “%1”.").arg(name)) }
            function onTaskDeleted(content) { win.toastShow(qsTr("Deleted “%1”.").arg(content)) }
            function onSectionDeleted(name) { win.toastShow(qsTr("Deleted the section “%1”.").arg(name)) }
            function onSectionCreated(name) { win.toastShow(qsTr("Added the section “%1”.").arg(name)) }
            function onProjectMoved(name) { win.toastShow(qsTr("Moved “%1”. Ctrl+Z to undo.").arg(name)) }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onVisibleChanged: if (!visible) win.helpVisible = false
        }

        Dialog {
            id: signOutDialog
            anchors.centerIn: parent
            modal: true
            title: qsTr("Sign out of Todoist?")
            standardButtons: Dialog.Cancel | Dialog.Ok
            Material.roundedScale: theme.radius
            Label {
                text: qsTr("Zero forgets the API token. Your tasks stay in Todoist.")
                wrapMode: Text.Wrap
                width: 320
            }
            onAccepted: backend.signOut()
        }
    }

    // Where a dragged row would land.
    component DropLine: Rectangle {
        height: 2
        radius: 1
        color: theme.accent
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: -4
            width: 8
            height: 8
            radius: 4
            color: theme.accent
        }
    }

    component SidebarItem: Item {
        id: item
        property string view: ""
        property string label: ""
        property string glyph: ""
        property string hotkey: ""
        property color dotColor: "transparent"
        property int indent: 0
        property int count: 0
        // Set for projects, which get a menu; the fixed lists don't.
        property var project: null
        readonly property bool current: backend.view === view
        // A dragged task would land here.
        readonly property bool dropHere: taskList.dropTarget !== null && taskList.dropTarget.view === view

        function openMenu() {
            projectMenu.project = item.project
            projectMenu.popup()
        }

        Layout.fillWidth: true
        implicitHeight: 36

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            radius: Math.min(6, theme.radius)
            color: item.dropHere ? Qt.alpha(theme.accent, 0.18) : item.current ? theme.tint(0.08)
                 : itemHover.hovered ? theme.tint(0.04) : "transparent"
            border.width: item.dropHere ? 1 : 0
            border.color: theme.accent
        }
        Rectangle {
            visible: item.current
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 18
            color: theme.accent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20 + item.indent * 14
            anchors.rightMargin: 20
            spacing: 10

            ListIcon {
                visible: item.glyph !== ""
                kind: item.glyph
                color: item.current ? theme.accent : theme.softForeground
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }
            // In an icon-wide slot, so project names line up with the lists'.
            Item {
                visible: item.glyph === ""
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                Rectangle {
                    anchors.centerIn: parent
                    width: 9
                    height: 9
                    radius: 4.5
                    color: item.dotColor
                }
            }
            Label {
                Layout.fillWidth: true
                text: item.label
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 14
                color: item.current ? theme.foreground : theme.softForeground
            }
            Label {
                visible: item.count > 0
                text: item.count.toLocaleString(Qt.locale(), 'f', 0)
                font.pixelSize: 12
                color: theme.dimForeground
            }
            Label {
                visible: item.project !== null && (itemHover.hovered || item.current)
                text: "⋯"
                font.pixelSize: 16
                leftPadding: 4
                rightPadding: 4
                color: moreHover.hovered ? theme.accent : theme.dimForeground
                HoverHandler { id: moreHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: item.openMenu() }
            }
        }

        HoverHandler { id: itemHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: { backend.showView(item.view); win.focusList() } }
        opacity: item.project && projectList.dragProject && projectList.dragProject.id === item.project.id ? 0.35 : 1
        DragHandler {
            target: null
            enabled: item.project !== null
            cursorShape: Qt.ClosedHandCursor
            onActiveChanged: {
                if (active) projectList.dragProject = item.project
                else projectList.dropDragged()
            }
            onCentroidChanged: if (active) projectList.dragMoved(centroid.scenePosition)
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: item.project !== null
            onTapped: item.openMenu()
        }
    }
}
