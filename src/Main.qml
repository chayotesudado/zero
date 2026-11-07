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
    readonly property bool pickerOpen: movePicker.opened || schedulePicker.opened
                                       || newProjectDialog.opened || deleteProjectDialog.opened
                                       || projectColorDialog.opened
    readonly property bool keysLive: backend.signedIn && !typing && !helpVisible && !pickerOpen
    readonly property string today: Dates.isoToday()

    // The lists in the order [ and ] walk through them.
    readonly property var views: {
        var list = ["inbox", "today", "upcoming"]
        for (var i = 0; i < backend.projects.length; i++)
            list.push("project:" + backend.projects[i].id)
        return list
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

    function selectedTask() {
        return taskList.currentIndex >= 0 && taskList.currentIndex < backend.tasks.length
            ? backend.tasks[taskList.currentIndex] : null
    }

    function completeSelected() {
        var item = taskList.itemAtIndex(taskList.currentIndex)
        if (item) item.complete()
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: !win.pickerOpen && (win.helpVisible || addField.activeFocus)
        onActivated: { if (win.helpVisible) win.helpVisible = false; else { addField.text = ""; taskList.forceActiveFocus() } } }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.completeSelected() }
    Shortcut { sequences: ["N", "A"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: addField.forceActiveFocus() }
    Shortcut { sequences: ["J", "Down"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: taskList.moveSelection(1) }
    Shortcut { sequences: ["K", "Up"]; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: taskList.moveSelection(-1) }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: win.keysLive
        onActivated: { var t = win.selectedTask(); if (t) Qt.openUrlExternally(t.url) } }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: movePicker.openFor(win.selectedTask()) }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: schedulePicker.openFor(win.selectedTask()) }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.keysLive
        onActivated: { var t = win.selectedTask(); if (t) backend.schedule(t.id, "today") } }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: newProjectDialog.openNew() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.keysLive && backend.view.indexOf("project:") === 0
        onActivated: projectColorDialog.openFor(win.currentProject()) }
    Shortcut { sequence: "Ctrl+Delete"; context: Qt.ApplicationShortcut; enabled: win.keysLive && backend.view.indexOf("project:") === 0
        onActivated: deleteProjectDialog.openFor(win.currentProject()) }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.refresh() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.showView("inbox") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.showView("today") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: backend.showView("upcoming") }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.stepView(-1) }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: win.keysLive; onActivated: win.stepView(1) }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: backend.signedIn && !win.typing; onActivated: backend.undo() }
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

                    SidebarItem { view: "inbox"; label: qsTr("Inbox"); glyph: "inbox"; count: backend.inboxCount; hotkey: "1" }
                    SidebarItem { view: "today"; label: qsTr("Today"); glyph: "today"; count: backend.todayCount; hotkey: "2" }
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
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: backend.projects
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: SidebarItem {
                            required property var modelData
                            width: ListView.view.width
                            view: "project:" + modelData.id
                            label: modelData.name
                            dotColor: modelData.color
                            indent: modelData.depth
                            project: modelData
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
                    placeholderText: backend.view === "today" ? qsTr("Add to Today (N)")
                                   : backend.view === "upcoming" ? qsTr("Add a task (N)")
                                   : qsTr("Add to %1 (N)").arg(backend.viewName)
                    background: Rectangle {
                        radius: Math.min(6, theme.radius)
                        color: addField.activeFocus ? theme.tint(0.04) : "transparent"
                        border.width: 1
                        border.color: addField.activeFocus ? theme.accent : theme.lighterBackground
                    }
                    onAccepted: {
                        if (text.trim() === "") return
                        backend.add(text)
                        text = ""
                    }
                }
                Label {
                    Layout.leftMargin: 29
                    Layout.topMargin: 4
                    Layout.bottomMargin: 8
                    text: addField.activeFocus
                          ? (backend.view === "upcoming"
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
                    model: backend.tasks
                    currentIndex: -1
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    // The selection follows the task, not the row, across
                    // refreshes. Only the user moves it; a model reset can move
                    // currentIndex on its own, so it's restored after one.
                    property string selectedId: ""
                    function select(i) {
                        var tasks = backend.tasks
                        if (tasks.length === 0) { currentIndex = -1; selectedId = ""; return }
                        currentIndex = Math.max(0, Math.min(i, tasks.length - 1))
                        selectedId = tasks[currentIndex].id
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                    function moveSelection(delta) { select(currentIndex < 0 ? 0 : currentIndex + delta) }
                    function restoreSelection(previous) {
                        var tasks = backend.tasks
                        for (var i = 0; i < tasks.length; i++)
                            if (tasks[i].id === selectedId) { currentIndex = i; return }
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

                    delegate: TaskRow {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        task: modelData
                        selected: ListView.isCurrentItem
                        showProject: taskList.grouped
                        onSelectRequested: { taskList.select(index); taskList.forceActiveFocus() }
                        onCompleteRequested: backend.complete(modelData.id)
                        onScheduleRequested: { taskList.select(index); schedulePicker.openFor(modelData) }
                        onMoveRequested: { taskList.select(index); movePicker.openFor(modelData) }
                    }

                    Column {
                        anchors.centerIn: parent
                        visible: backend.loaded && backend.tasks.length === 0
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

        MovePicker {
            id: movePicker
            onClosed: taskList.forceActiveFocus()
        }

        SchedulePicker {
            id: schedulePicker
            onClosed: taskList.forceActiveFocus()
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
                text: qsTr("Change color…")
                onTriggered: projectColorDialog.openFor(projectMenu.project)
            }
            MenuItem {
                text: qsTr("Delete project…")
                onTriggered: deleteProjectDialog.openFor(projectMenu.project)
            }
        }

        Connections {
            target: backend
            function onProjectDeleted(name) {
                toast.message = qsTr("Deleted “%1”.").arg(name)
                toastTimer.restart()
            }
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
            color: item.current ? theme.tint(0.08) : itemHover.hovered ? theme.tint(0.04) : "transparent"
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
        TapHandler { onTapped: { backend.showView(item.view); taskList.forceActiveFocus() } }
        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: item.project !== null
            onTapped: item.openMenu()
        }
    }
}
