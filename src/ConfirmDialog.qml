pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Zero

// Asks before something that can't be undone. Enter answers with whichever
// button has focus: Delete for a single task, so Delete then Enter is quick,
// Cancel when more goes with it.
Popup {
    id: dialog

    property string title: ""
    property string message: ""
    property string confirmText: qsTr("Delete")
    signal confirmed()

    function ask(heading, body, focusConfirm) {
        title = heading
        message = body
        open()
        if (focusConfirm) confirmButton.forceActiveFocus()
        else cancelButton.forceActiveFocus()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(440, parent ? parent.width - 48 : 440)
    padding: 20
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Theme.light ? 0.3 : 0.6) }
    background: Rectangle { color: Theme.darkBackground; border.color: Theme.lighterBackground; radius: Theme.radius }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: dialog.title
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 16
            font.bold: true
            color: Theme.foreground
        }

        Label {
            Layout.fillWidth: true
            visible: text !== ""
            text: dialog.message
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.softForeground
            font.pixelSize: 14
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.red
            font.pixelSize: 13
            text: qsTr("This can't be undone.")
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                id: cancelButton
                text: qsTr("Cancel")
                flat: true
                onClicked: dialog.close()
                Keys.onReturnPressed: dialog.close()
                Keys.onEnterPressed: dialog.close()
            }
            Button {
                id: confirmButton
                text: dialog.confirmText
                highlighted: true
                contentItem: Label {
                    text: confirmButton.text
                    color: Theme.textOn(Theme.red)
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 96
                    implicitHeight: 38
                    radius: Math.min(6, Theme.radius)
                    color: Theme.red
                    opacity: confirmButton.down ? 0.8 : 1
                    border.width: confirmButton.activeFocus ? 2 : 0
                    border.color: Theme.foreground
                }
                function confirm() { dialog.close(); dialog.confirmed() }
                onClicked: confirm()
                Keys.onReturnPressed: confirm()
                Keys.onEnterPressed: confirm()
            }
        }
    }
}
