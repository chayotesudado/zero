pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import Zero
import "Dates.js" as Dates

ApplicationWindow {
    id: win
    width: 1000
    height: 700
    minimumWidth: 360
    minimumHeight: 460
    visible: true
    title: Backend.signedIn && Backend.viewName ? qsTr("%1 — Zero").arg(Backend.viewName) : "Zero"
    color: Theme.background

    Material.theme: Theme.light ? Material.Light : Material.Dark
    Material.accent: Theme.accent
    Material.foreground: Theme.foreground
    Material.background: Theme.background
    // Material's corners as near as its scale gets to Hyprland's rounding;
    // controls drawn here use Theme.radius itself.
    Material.roundedScale: Theme.radius >= 16 ? Material.LargeScale : Theme.radius >= 12 ? Material.MediumScale
                         : Theme.radius >= 8 ? Material.SmallScale : Theme.radius >= 4 ? Material.ExtraSmallScale
                         : Material.NotRounded

    property bool helpVisible: false
    // Narrow, the sidebar steps aside: ☰ (or Ctrl+B) slides it back over
    // the list, and picking a list, Esc, or a tap beside it puts it away.
    readonly property bool compact: width < 720
    property bool sidebarOpen: false

    // The pixel art's three tones, from the theme: the text color for the
    // faces, fading to the muted color for the depth behind them.
    readonly property color pixelFace: Theme.foreground
    readonly property color pixelMid: Theme.mix(Theme.foreground, Theme.muted, 0.5)
    readonly property color pixelDeep: Theme.muted
    // The line the logo and the page title share: from the window's top,
    // and as tall as the logo.
    readonly property int headerTop: 22
    readonly property int headerHeight: 30
    // A project's color as a pastel: its hue, gentler, and light on dark
    // themes (dark on light ones) so the title reads on either.
    function pastel(c) {
        var k = Qt.color(c)
        var light = Theme.light ? Math.min(k.hslLightness, 0.42) : Math.max(k.hslLightness, 0.67)
        return Qt.hsla(k.hslHue, Math.min(k.hslSaturation, 0.75) * 0.88, light, 1)
    }
    onCompactChanged: sidebarOpen = false
    // Single-key shortcuts stand down while typing into a field.
    readonly property bool typing: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    // Visible, not opened: a popup counts from the moment it starts to open,
    // since opened waits for its animation, and a click that opened one
    // mustn't hand the keyboard back to the list in the meantime.
    readonly property bool pickerOpen: movePicker.visible || schedulePicker.visible
                                       || newProjectDialog.visible || deleteProjectDialog.visible
                                       || projectColorDialog.visible || sectionDialog.visible || confirmDialog.visible
                                       || taskDetail.visible || projectMenu.visible
    readonly property bool keysLive: Backend.signedIn && !typing && !helpVisible && !pickerOpen
    // The task details are open with nothing over them. Then they take a
    // few of the list's keys for the task they show: Ctrl+1–4 and
    // Ctrl+Enter even while typing in them, D and M (detailKeys) when not.
    readonly property bool detailOnTop: taskDetail.visible && !movePicker.visible && !schedulePicker.visible
                                        && !confirmDialog.visible
    readonly property bool detailKeys: detailOnTop && !typing
    readonly property bool inProject: Backend.view.indexOf("project:") === 0
    readonly property string today: Dates.isoToday()
    // Projects and the Inbox have sections; the dated lists don't.
    readonly property bool sectionsHere: Backend.view === "inbox" || Backend.view.indexOf("project:") === 0

    // The lists in the order [ and ] walk through them.
    readonly property var views: {
        var list = ["inbox", "today", "upcoming"]
        for (var i = 0; i < Backend.projects.length; i++)
            list.push("project:" + Backend.projects[i].id)
        return list
    }

    function toastShow(message) {
        toast.message = message
        toastTimer.restart()
    }

    function stepView(delta) {
        var i = views.indexOf(Backend.view)
        Backend.showView(views[(i + delta + views.length) % views.length])
    }

    function currentProject() {
        var list = Backend.projects
        for (var i = 0; i < list.length; i++)
            if ("project:" + list[i].id === Backend.view) return list[i]
        return null
    }

    // The selected row: a task, or a section's heading.
    function selectedRow() {
        var rows = Backend.rows
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
        if (row.kind === "section") Backend.moveSectionBy(row.id, delta)
        else Backend.moveTaskBy(row.id, delta)
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
    // inbox is the Inbox's id, looked up once when the drag starts.
    function sidebarTargetAt(scenePoint, inbox) {
        var fixed = [[inboxItem, inbox], [todayItem, ""]]
        for (var i = 0; i < fixed.length; i++) {
            var item = fixed[i][0]
            var p = item.mapFromItem(null, scenePoint.x, scenePoint.y)
            if (p.x >= 0 && p.y >= 0 && p.x < item.width && p.y < item.height)
                return { view: item.view, projectId: fixed[i][1], name: item.label }
        }
        var q = projectList.mapFromItem(null, scenePoint.x, scenePoint.y)
        if (q.x < 0 || q.y < 0 || q.x >= projectList.width || q.y >= projectList.height) return null
        var index = projectList.indexAt(q.x, q.y + projectList.contentY)
        var item = index < 0 ? null : projectList.itemAtIndex(index)
        if (!item || !item.project) return null
        return { view: "project:" + item.project.id, projectId: item.project.id, name: item.project.name }
    }

    function inboxId() {
        var list = Backend.destinations
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

    function moveTask(task) {
        if (!task) return
        movePicker.target = task
        movePicker.mode = "task"
        movePicker.show(qsTr("Move “%1” to").arg(task.content), Backend.destinations,
                        task.sectionId ? "section:" + task.sectionId : "project:" + task.projectId)
    }

    function moveProject(project) {
        if (!project) return
        movePicker.target = project
        movePicker.mode = "project"
        movePicker.show(qsTr("Move “%1” under").arg(project.name), Backend.parentChoices(project.id),
                        project.parentId ? "project:" + project.parentId : "top")
    }

    // Undoing an add deletes the task from Todoist for good, so it asks
    // first, like Delete does.
    function undo() {
        var doomed = Backend.undoDeletes()
        if (!doomed.id) { Backend.undo(); return }
        var subtasks = Backend.subtaskCount(doomed.id)
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
            // A project's list has all its tasks, so the count is exact. A
            // dated list (and the details opened from one) may not show
            // them all, so Cancel takes Enter there.
            var subtasks = Backend.subtaskCount(row.id)
            var exact = win.sectionsHere
            confirmDialog.ask(qsTr("Delete “%1”?").arg(row.content),
                              subtasks > 0 ? qsTr("%n subtasks will be deleted with it.", "", subtasks)
                                           : exact ? "" : qsTr("Any subtasks it has will be deleted with it."),
                              subtasks === 0 && exact)
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut
        enabled: !win.pickerOpen && (win.helpVisible || addField.activeFocus || (win.compact && win.sidebarOpen))
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (addField.activeFocus) { addField.text = ""; addField.parentTask = null; taskList.forceActiveFocus() }
            else win.sidebarOpen = false
        } }
    Shortcut { sequence: "Ctrl+B"; context: Qt.ApplicationShortcut; enabled: win.compact && Backend.signedIn && !win.pickerOpen
        onActivated: win.sidebarOpen = !win.sidebarOpen }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.completeSelected() }
    Shortcut { sequences: ["N", "A"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: addField.forceActiveFocus() }
    Shortcut { sequences: ["J", "Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: taskList.moveSelection(1) }
    Shortcut { sequences: ["K", "Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: taskList.moveSelection(-1) }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: win.keysLive
        onActivated: { var t = win.selectedTask(); if (t) Qt.openUrlExternally(t.url) } }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: win.keysLive || win.detailKeys; onActivated: win.moveTask(win.targetTask()) }
    Shortcut { sequence: "Shift+M"; context: Qt.ApplicationShortcut; enabled: win.keysLive && Backend.view.indexOf("project:") === 0
        onActivated: win.moveProject(win.currentProject()) }
    Shortcut { sequences: ["Delete", "Backspace"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.deleteRow(win.selectedRow()) }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: sectionDialog.openNew(Backend.viewName) }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: win.keysLive || win.detailKeys; onActivated: schedulePicker.openFor(win.targetTask()) }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.keysLive
        onActivated: { var t = win.selectedTask(); if (t) Backend.schedule(t.id, "today") } }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: newProjectDialog.openNew() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.keysLive && Backend.view.indexOf("project:") === 0
        onActivated: projectColorDialog.openFor(win.currentProject()) }
    Shortcut { sequence: "Ctrl+Delete"; context: Qt.ApplicationShortcut; enabled: win.keysLive && Backend.view.indexOf("project:") === 0
        onActivated: deleteProjectDialog.openFor(win.currentProject()) }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: Backend.refresh() }
    Shortcut { sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.openSelected() }
    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; enabled: win.detailOnTop
        onActivated: taskDetail.close() }
    Shortcut { sequence: "Shift+N"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.addSubtask() }
    Shortcut { sequences: ["Ctrl+]", "Tab"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: { var t = win.selectedTask(); if (t) Backend.indentTask(t.id) } }
    Shortcut { sequences: ["Ctrl+[", "Shift+Tab", "Backtab"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: { var t = win.selectedTask(); if (t) Backend.outdentTask(t.id) } }
    Shortcut { sequences: ["Alt+J", "Alt+Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: win.moveSelectedBy(1) }
    Shortcut { sequences: ["Alt+K", "Alt+Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.sectionsHere
        onActivated: win.moveSelectedBy(-1) }
    Shortcut { sequences: ["Alt+Shift+J", "Alt+Shift+Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.inProject
        onActivated: { var p = win.currentProject(); if (p) Backend.moveProjectBy(p.id, 1) } }
    Shortcut { sequences: ["Alt+Shift+K", "Alt+Shift+Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive && win.inProject
        onActivated: { var p = win.currentProject(); if (p) Backend.moveProjectBy(p.id, -1) } }
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
                enabled: win.keysLive || win.detailOnTop
                onActivated: { var t = win.targetTask(); if (t) Backend.setPriority(t.id, index + 1) }
            }
        }
    }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: Backend.showView("inbox") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: Backend.showView("today") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: Backend.showView("upcoming") }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.stepView(-1) }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.stepView(1) }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: Backend.signedIn && !win.typing; onActivated: win.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: Backend.signedIn && !win.typing; onActivated: Backend.redo() }

    Component.onCompleted: if (Backend.signedIn) taskList.forceActiveFocus(); else signIn.focusField()

    Connections {
        target: Backend
        function onViewChanged() { win.sidebarOpen = false }
    }

    // Coming back to the window picks up changes made elsewhere.
    onActiveChanged: if (active && Backend.signedIn) Backend.refresh()

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        SignInView {
            id: signIn
            anchors.fill: parent
            visible: !Backend.signedIn
            // Only the visible screen takes the keyboard, so a hidden field
            // never swallows the single-key shortcuts.
            onVisibleChanged: if (visible) focusField(); else taskList.forceActiveFocus()
        }

        Item {
            anchors.fill: parent
            visible: Backend.signedIn

            // ------------------------------------------------ sidebar
            Rectangle {
                id: sidebar
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: !win.compact || win.sidebarOpen ? 0 : -width - 1
                Behavior on anchors.leftMargin { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                width: 230
                z: 3
                color: Theme.darkBackground

                ColumnLayout {
                    id: sidebarColumn
                    anchors.fill: parent
                    anchors.topMargin: 18
                    anchors.bottomMargin: 12
                    spacing: 2

                    // The logo, the check and ZERO, drawn in the theme's
                    // colors at three times its pixel art (165 by 30), lined
                    // up with the lists' icons below. It shares its line with
                    // the page title, and below it there's 1.5 times the gap
                    // between the lists, so it reads as a header, not the
                    // first item.
                    PixelArt {
                        art: "lockup"
                        pixelScale: 3
                        faceColor: win.pixelFace
                        midColor: win.pixelMid
                        deepColor: win.pixelDeep
                        Layout.leftMargin: 20
                        Layout.topMargin: win.headerTop - sidebarColumn.anchors.topMargin
                        Layout.bottomMargin: 22
                        Accessible.role: Accessible.StaticText
                        Accessible.name: "Zero"
                    }

                    SidebarItem { id: inboxItem; view: "inbox"; label: qsTr("Inbox"); glyph: "inbox"; hotkey: "1" }
                    SidebarItem { id: todayItem; view: "today"; label: qsTr("Today"); glyph: "today"; count: Backend.todayCount; hotkey: "2" }
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
                            color: Theme.muted
                        }
                        Label {
                            text: "+"
                            font.pixelSize: 18
                            leftPadding: 6
                            rightPadding: 6
                            color: addProjectHover.hovered ? Theme.accent : Theme.dimForeground
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
                        model: Backend.projects
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
                            dropAt = win.dropIndexIn(projectList, projectList.count, p.y)
                        }
                        function dropDragged() {
                            if (dragProject && dropAt >= 0) Backend.dropProject(dragProject.id, dropAt)
                            dragProject = null
                            dropAt = -1
                        }

                        DropLine {
                            visible: projectList.dragProject !== null && projectList.dropAt >= 0
                            y: Math.max(0, (projectList.dropAt, projectList.contentHeight,
                                win.dropLineY(projectList, projectList.count, projectList.dropAt)) - 1)
                            x: 20 + (projectList.dragProject ? projectList.dragProject.depth : 0) * 14
                            width: projectList.width - x - 12
                        }
                    }

                    Label {
                        Layout.leftMargin: 20
                        visible: !Backend.tokenFromEnvironment
                        text: qsTr("Sign out")
                        font.pixelSize: 12
                        color: signOutHover.hovered ? Theme.softForeground : Theme.muted
                        HoverHandler { id: signOutHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: signOutDialog.open() }
                    }
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: sidebar.right
                width: 1
                z: 3
                color: Theme.lighterBackground
            }

            // Behind the slid-out sidebar: a tap here puts it away.
            Rectangle {
                anchors.fill: parent
                z: 2
                visible: win.compact && win.sidebarOpen
                color: Qt.rgba(0, 0, 0, Theme.light ? 0.25 : 0.5)
                TapHandler { onTapped: win.sidebarOpen = false }
            }

            // ------------------------------------------------ list
            ColumnLayout {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.leftMargin: win.compact ? 0 : sidebar.width + 1
                spacing: 0

                RowLayout {
                    id: pageHeader
                    Layout.fillWidth: true
                    // The title starts where the add box's text does, as the
                    // logo starts where the lists' icons do.
                    Layout.leftMargin: win.compact ? 16 : 28
                    Layout.rightMargin: win.compact ? 16 : 24
                    Layout.topMargin: win.headerTop
                    Layout.preferredHeight: win.headerHeight
                    spacing: 12

                    Label {
                        visible: win.compact
                        text: "☰"
                        font.pixelSize: 20
                        padding: 2
                        color: menuHover.hovered ? Theme.accent : Theme.softForeground
                        HoverHandler { id: menuHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: win.sidebarOpen = !win.sidebarOpen }
                        ToolTip.visible: menuHover.hovered && !win.sidebarOpen
                        ToolTip.text: qsTr("Lists (Ctrl+B)")
                        ToolTip.delay: 400
                    }
                    // The list's name, its capitals middled on the logo's
                    // line. A project's is in its color, softened toward
                    // the text color so it reads on any Theme.
                    Item {
                        id: pageTitle
                        Layout.fillWidth: true
                        Layout.preferredHeight: win.headerHeight
                        // The middle of the logo's faces, 12 px down.
                        readonly property real faceMiddle: 12
                        readonly property var project: win.currentProject()

                        Label {
                            id: titleLabel
                            width: parent.width
                            text: Backend.viewName
                            textFormat: Text.PlainText
                            font.pixelSize: 20
                            font.bold: true
                            color: pageTitle.project && pageTitle.project.color
                                   ? win.pastel(pageTitle.project.color) : Theme.foreground
                            elide: Text.ElideRight
                            Accessible.role: Accessible.Heading
                            FontMetrics { id: titleMetrics; font: titleLabel.font }
                            // Given the font, so it re-measures when that
                            // changes; the call alone isn't a dependency.
                            function capHeightOf(font: font): real { return titleMetrics.tightBoundingRect("H").height }
                            readonly property real capHeight: capHeightOf(titleMetrics.font)
                            readonly property real wantedY: pageTitle.faceMiddle - baselineOffset + capHeight / 2
                            y: isFinite(wantedY) ? Math.round(wantedY) : 0
                        }
                    }
                    BusyIndicator {
                        running: Backend.busy
                        opacity: running ? 1 : 0
                        implicitWidth: 28
                        implicitHeight: 28
                    }
                    Label {
                        visible: !win.compact
                        text: qsTr("? for shortcuts")
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                }

                // A failed request says why, and stays until dismissed or fixed.
                Rectangle {
                    id: errorBanner
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 24
                    Layout.topMargin: 12
                    visible: Backend.error !== ""
                    implicitHeight: errorRow.implicitHeight + 20
                    radius: Math.min(6, Theme.radius)
                    color: Theme.mix(Theme.red, Theme.background, 0.16)
                    border.color: Theme.mix(Theme.red, Theme.background, 0.45)

                    RowLayout {
                        id: errorRow
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.leftMargin: 14
                        Label {
                            Layout.fillWidth: true
                            text: Backend.error
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: Theme.red
                            font.pixelSize: 13
                        }
                        Button { text: qsTr("Retry"); flat: true; onClicked: { Backend.dismissError(); Backend.refresh() } }
                        Button { text: qsTr("Dismiss"); flat: true; onClicked: Backend.dismissError() }
                    }
                }

                // A plain field sized like a task row; Material's own reserves
                // room for a floating label and stands much taller.
                Basic.TextField {
                    id: addField
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    // Level with Inbox in the lists, and as tall, unless the
                    // error banner is in the way.
                    Layout.topMargin: errorBanner.visible ? 12
                        : Math.max(8, sidebarColumn.anchors.topMargin + inboxItem.y - win.headerTop - win.headerHeight)
                    implicitHeight: inboxItem.height
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 0
                    bottomPadding: 0
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 13
                    color: Theme.foreground
                    selectionColor: Theme.selection
                    selectedTextColor: Theme.foreground
                    // Set by Shift+N: what's added goes under this task,
                    // until Esc or the box is left.
                    property var parentTask: null
                    // Where a task lands, as quick-add text the box starts
                    // with: "today ", "#Project ", "#Project /Section ".
                    // Typing after it files the task there; clearing it
                    // sends the task to the Inbox. Put in when the box is
                    // entered, and taken out when it's left untouched, so
                    // the box at rest just says what it's for.
                    property string prefill: ""
                    // Into the box, starting from the selected row's place,
                    // even if it holds another place's untouched prefill.
                    function start() {
                        if (text === prefill) text = ""
                        if (activeFocus) fill()
                        else forceActiveFocus()
                    }
                    function fill() {
                        prefill = parentTask ? "" : Backend.addPrefix(win.addSection())
                        text = prefill
                        cursorPosition = text.length
                    }
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            if (text === "") fill()
                        } else {
                            if (text === prefill) text = ""
                            parentTask = null
                        }
                    }
                    // Drawn here rather than as placeholderText, which would
                    // read a task's name as markup.
                    Label {
                        x: addField.leftPadding
                        width: addField.width - addField.leftPadding - addField.rightPadding
                        anchors.verticalCenter: parent.verticalCenter
                        visible: addField.length === 0 && addField.preeditText === ""
                        text: addField.parentTask ? qsTr("Add a subtask to “%1”").arg(addField.parentTask.content) : qsTr("Add a task (N)")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font: addField.font
                        color: Theme.muted
                    }
                    background: Rectangle {
                        radius: Math.min(6, Theme.radius)
                        color: addField.activeFocus ? Theme.tint(0.04) : "transparent"
                        border.width: 1
                        border.color: addField.activeFocus ? Theme.accent : Theme.lighterBackground
                    }
                    onAccepted: {
                        if (text.trim() === "" || text === prefill) return
                        Backend.add(text, parentTask ? parentTask.id : "")
                        // Ready for the next one, in the same place.
                        fill()
                    }
                }
                Label {
                    Layout.leftMargin: 29
                    Layout.topMargin: 4
                    Layout.bottomMargin: 8
                    text: addField.activeFocus
                          ? (addField.parentTask ? qsTr("Goes under “%1”, wherever it is. Dates and p1–p4 work. Esc to stop.").arg(addField.parentTask.content)
                             : addField.prefill === ""
                             ? qsTr("Quick add: “Pay rent friday 9am #Home p1 @errand”. Without a #project it goes to the Inbox.")
                             : qsTr("Type after “%1” to add it there, or clear it for the Inbox. Dates, #project, @label and p1–p4 work.").arg(addField.prefill.trim()))
                          : " "
                    // It quotes task and project names, which aren't markup.
                    textFormat: Text.PlainText
                    font.pixelSize: 11
                    color: Theme.muted
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
                    model: Backend.rows
                    currentIndex: -1
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    // The selection follows the task, not the row, across
                    // refreshes. Only the user moves it; a model reset can move
                    // currentIndex on its own, so it's restored after one.
                    property string selectedId: ""
                    function select(i) {
                        var rows = Backend.rows
                        if (rows.length === 0) { currentIndex = -1; selectedId = ""; return }
                        currentIndex = Math.max(0, Math.min(i, rows.length - 1))
                        selectedId = rows[currentIndex].key
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                    function moveSelection(delta) { select(currentIndex < 0 ? 0 : currentIndex + delta) }
                    function restoreSelection(previous) {
                        var rows = Backend.rows
                        for (var i = 0; i < rows.length; i++)
                            if (rows[i].key === selectedId) { currentIndex = i; return }
                        if (selectedId === "") { currentIndex = -1; return }
                        select(previous)
                    }
                    Connections {
                        target: Backend
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
                    // The rows and the Inbox's id as the drag began, so
                    // moving the pointer doesn't rebuild them from C++.
                    property var dragRows: []
                    property string dragInbox: ""
                    property int dropAt: -1
                    property var dropTarget: null
                    property point dragPoint: Qt.point(0, 0)
                    property point dragScene: Qt.point(0, 0)
                    function dragMoved(scenePoint) {
                        dragScene = scenePoint
                        // Narrow, the sidebar hides; nearing the left edge
                        // brings it out to drop on.
                        if (win.compact && !win.sidebarOpen && scenePoint.x < 28) win.sidebarOpen = true
                        dragPoint = mapFromItem(null, scenePoint.x, scenePoint.y)
                        var target = dragRow && dragRow.kind === "task" ? win.sidebarTargetAt(scenePoint, dragInbox) : null
                        // Its own project isn't somewhere else to go.
                        dropTarget = target && target.view !== "today" && target.projectId === dragRow.projectId ? null : target
                        var inList = dragPoint.x >= 0 && dragPoint.x < width
                        dropAt = !dropTarget && inList && win.sectionsHere
                                 ? win.dropIndexIn(taskList, count, dragPoint.y) : -1
                    }
                    function dropDragged() {
                        var row = dragRow, at = dropAt, target = dropTarget
                        dragRow = null
                        dragRows = []
                        dropAt = -1
                        dropTarget = null
                        if (win.compact) win.sidebarOpen = false
                        if (!row) return
                        if (target) {
                            if (target.view === "today") {
                                if (row.dayOffset !== 0 || !row.hasDue) {
                                    Backend.schedule(row.id, "today")
                                    win.toastShow(qsTr("“%1” is due today. Ctrl+Z to undo.").arg(row.content))
                                }
                            } else if (target.projectId && target.projectId !== row.projectId) {
                                Backend.moveTask(row.id, target.projectId, "")
                                win.toastShow(qsTr("Moved “%1” to %2. Ctrl+Z to undo.").arg(row.content).arg(target.name))
                            }
                            return
                        }
                        if (at < 0) return
                        if (row.kind === "section") Backend.dropSection(row.id, at)
                        else Backend.dropTask(row.id, at)
                    }
                    // The level a task would take, to indent the line by: the
                    // task it lands in front of, past itself and its subtasks,
                    // as the backend reads a drop.
                    readonly property int dropDepth: {
                        var rows = dragRows
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
                            taskList.dropAt = win.dropIndexIn(taskList, taskList.count, taskList.dragPoint.y)
                        }
                    }

                    DropLine {
                        visible: taskList.dragRow !== null && taskList.dropAt >= 0
                        y: Math.max(0, (taskList.dropAt, taskList.contentHeight,
                            win.dropLineY(taskList, taskList.count, taskList.dropAt)) - 1)
                        x: 16 + taskList.dropDepth * 26
                        width: taskList.width - x - 16
                        z: 5
                    }

                    readonly property bool grouped: Backend.view === "today" || Backend.view === "upcoming"
                    // Upcoming is headed by day. Today isn't: it's all today,
                    // and an overdue task says its date in red.
                    section.property: Backend.view === "upcoming" ? "group" : ""
                    section.delegate: Item {
                        required property string section
                        width: ListView.view.width
                        height: 34
                        Label {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 20
                            anchors.bottomMargin: 6
                            text: Dates.sectionLabel(parent.section, win.today, Translator.locale)
                            font.pixelSize: 12
                            font.bold: true
                            color: parent.section === "overdue" ? Theme.red : Theme.softForeground
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            height: 1
                            color: Theme.lighterBackground
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
                        height: loader.implicitHeight
                        opacity: taskList.dragRow && taskList.dragRow.key === modelData.key ? 0.35 : 1

                        function complete() { if (!isSection) (loader.item as TaskRow)?.complete() }

                        // Dragging picks the row up; it lands where the line shows.
                        DragHandler {
                            target: null
                            enabled: win.sectionsHere || !entry.isSection
                            cursorShape: Qt.ClosedHandCursor
                            onActiveChanged: {
                                if (active) {
                                    taskList.select(entry.index)
                                    taskList.forceActiveFocus()
                                    taskList.dragRows = Backend.rows
                                    taskList.dragInbox = win.inboxId()
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
                                groupedByDay: taskList.grouped
                                narrow: taskList.width < 560
                                onSelectRequested: { taskList.select(entry.index); win.focusList() }
                                onCompleteRequested: Backend.complete(entry.modelData.id)
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
                                onAddRequested: { taskList.select(entry.index); addField.start() }
                                onDeleteRequested: { taskList.select(entry.index); win.deleteRow(entry.modelData) }
                                onRenameRequested: { taskList.select(entry.index); sectionDialog.openRenameSection(entry.modelData) }
                            }
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        visible: Backend.loaded && taskList.count === 0
                        spacing: 8
                        // The logo's check, nothing left to do.
                        PixelArt {
                            anchors.horizontalCenter: parent.horizontalCenter
                            art: "check"
                            pixelScale: 4
                            faceColor: win.pixelFace
                            midColor: win.pixelMid
                            deepColor: win.pixelDeep
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Backend.view === "today" ? qsTr("Nothing left for today.") : qsTr("Nothing here.")
                            font.pixelSize: 16
                            color: Theme.dimForeground
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
            radius: Math.min(8, Theme.radius)
            color: Theme.lighterBackground
            border.color: Theme.muted
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
                color: Theme.foreground
                font.pixelSize: 13
            }
            Timer { id: toastTimer; interval: 2600 }
            Connections {
                target: Backend
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
            radius: Math.min(6, Theme.radius)
            color: Theme.lighterBackground
            border.color: Theme.accent
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
                color: Theme.foreground
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
                if (mode === "project") Backend.moveProject(target.id, item.projectId)
                else Backend.moveTask(target.id, item.projectId, item.sectionId)
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
                    if (Backend.undoDeletes().id === pending.id) Backend.undo()
                } else if (pending.kind === "section") Backend.deleteSection(pending.id)
                else Backend.deleteTask(pending.id)
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
                onTriggered: Backend.moveProjectBy(projectMenu.project.id, -1)
            }
            MenuItem {
                text: qsTr("Move down")
                onTriggered: Backend.moveProjectBy(projectMenu.project.id, 1)
            }
            MenuItem {
                text: qsTr("Delete project…")
                onTriggered: deleteProjectDialog.openFor(projectMenu.project)
            }
        }

        Connections {
            target: Backend
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
            Label {
                text: qsTr("Zero forgets the API token. Your tasks stay in Todoist.")
                wrapMode: Text.Wrap
                width: 320
            }
            onAccepted: Backend.signOut()
        }
    }

    // Where a dragged row would land.
    component DropLine: Rectangle {
        height: 2
        radius: 1
        color: Theme.accent
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: -4
            width: 8
            height: 8
            radius: 4
            color: Theme.accent
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
        readonly property bool current: Backend.view === view
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
            radius: Math.min(6, Theme.radius)
            color: item.dropHere ? Qt.alpha(Theme.accent, 0.18) : item.current ? Theme.tint(0.08)
                 : itemHover.hovered ? Theme.tint(0.04) : "transparent"
            border.width: item.dropHere ? 1 : 0
            border.color: Theme.accent
        }
        Rectangle {
            visible: item.current
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 18
            color: Theme.accent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20 + item.indent * 14
            anchors.rightMargin: 20
            spacing: 10

            ListIcon {
                visible: item.glyph !== ""
                kind: item.glyph
                color: item.current ? Theme.accent : Theme.softForeground
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
                color: item.current ? Theme.foreground : Theme.softForeground
            }
            Label {
                visible: item.count > 0
                text: item.count.toLocaleString(Translator.locale, 'f', 0)
                font.pixelSize: 12
                color: Theme.dimForeground
            }
            Label {
                visible: item.project !== null && (itemHover.hovered || item.current)
                text: "⋯"
                font.pixelSize: 16
                leftPadding: 4
                rightPadding: 4
                color: moreHover.hovered ? Theme.accent : Theme.dimForeground
                HoverHandler { id: moreHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: item.openMenu() }
            }
        }

        HoverHandler { id: itemHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: { Backend.showView(item.view); win.focusList() } }
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
