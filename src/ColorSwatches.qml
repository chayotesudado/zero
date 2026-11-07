import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Todoist's 20 project colors, so a color picked here is the same one
// Todoist shows. Arrow keys move through them once the grid has focus;
// Enter or a click picks.
FocusScope {
    id: swatches

    property string selected: "charcoal"
    signal picked(string name)
    signal accepted()

    readonly property int columns: 10
    readonly property int count: backend.colors.length
    readonly property int selectedIndex: {
        for (var i = 0; i < backend.colors.length; i++)
            if (backend.colors[i].name === selected) return i
        return 0
    }

    // Todoist's own names for its colors.
    function label(name) {
        var labels = {
            berry_red: qsTr("Berry red"), red: qsTr("Red"), orange: qsTr("Orange"), yellow: qsTr("Yellow"),
            olive_green: qsTr("Olive green"), lime_green: qsTr("Lime green"), green: qsTr("Green"),
            mint_green: qsTr("Mint green"), teal: qsTr("Teal"), sky_blue: qsTr("Sky blue"),
            light_blue: qsTr("Light blue"), blue: qsTr("Blue"), grape: qsTr("Grape"), violet: qsTr("Violet"),
            lavender: qsTr("Lavender"), magenta: qsTr("Magenta"), salmon: qsTr("Salmon"),
            charcoal: qsTr("Charcoal"), grey: qsTr("Grey"), taupe: qsTr("Taupe")
        }
        return labels[name] || name
    }

    function step(delta) {
        var i = Math.max(0, Math.min(count - 1, selectedIndex + delta))
        selected = backend.colors[i].name
        picked(selected)
    }

    implicitWidth: grid.implicitWidth
    implicitHeight: grid.implicitHeight + nameLabel.implicitHeight + 8
    activeFocusOnTab: true

    Keys.onLeftPressed: step(LayoutMirroring.enabled ? 1 : -1)
    Keys.onRightPressed: step(LayoutMirroring.enabled ? -1 : 1)
    Keys.onUpPressed: step(-columns)
    Keys.onDownPressed: step(columns)
    Keys.onReturnPressed: accepted()
    Keys.onEnterPressed: accepted()

    Grid {
        id: grid
        columns: swatches.columns
        spacing: 8

        Repeater {
            model: backend.colors
            Item {
                required property var modelData
                readonly property bool chosen: modelData.name === swatches.selected
                width: 30
                height: 30

                // A ring marks the choice, and a second one the keyboard focus.
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: chosen ? (swatches.activeFocus ? theme.accent : theme.foreground) : "transparent"
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    radius: 11
                    color: modelData.hex
                    scale: swatchHover.hovered ? 1.12 : 1
                    Behavior on scale { NumberAnimation { duration: 90 } }
                }
                HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        swatches.selected = modelData.name
                        swatches.picked(modelData.name)
                        swatches.forceActiveFocus()
                    }
                }
            }
        }
    }

    Label {
        id: nameLabel
        anchors.top: grid.bottom
        anchors.topMargin: 8
        text: swatches.label(swatches.selected)
        color: theme.dimForeground
        font.pixelSize: 13
    }
}
