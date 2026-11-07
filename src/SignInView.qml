import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Shown until Zero has a Todoist API token. The token is checked against
// Todoist before it's saved.
Item {
    id: view

    property string message: ""
    property bool checking: false

    function focusField() { tokenField.forceActiveFocus() }

    Connections {
        target: backend
        function onSignInFailed(message) { view.checking = false; view.message = message }
        function onSignedInChanged() { view.checking = false; view.message = ""; tokenField.text = "" }
    }

    function submit() {
        if (checking)
            return
        message = ""
        checking = true
        backend.signIn(tokenField.text)
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 480)
        height: column.implicitHeight + 64
        radius: theme.radius
        color: theme.darkBackground
        border.color: theme.lighterBackground

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 32
            spacing: 14

            Label {
                text: qsTr("Connect Todoist")
                font.pixelSize: 24
                font.bold: true
                color: theme.foreground
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: theme.dimForeground
                font.pixelSize: 14
                textFormat: Text.StyledText
                linkColor: theme.accent
                text: qsTr("Paste your API token from Todoist under Settings → Integrations → Developer. It's kept in %1, readable only by you.")
                      .arg("<tt>~/.config/zero/token</tt>")
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: theme.accent
                font.pixelSize: 13
                text: "app.todoist.com/app/settings/integrations/developer"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.openUrlExternally("https://app.todoist.com/app/settings/integrations/developer")
                }
            }

            TextField {
                id: tokenField
                Layout.fillWidth: true
                Layout.topMargin: 6
                placeholderText: qsTr("API token")
                echoMode: TextInput.Password
                enabled: !view.checking
                Material.roundedScale: theme.radius
                onAccepted: view.submit()
            }

            Label {
                Layout.fillWidth: true
                visible: view.message !== ""
                text: view.message
                // Server text, shown as-is rather than parsed as HTML.
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: theme.red
                font.pixelSize: 13
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                BusyIndicator { running: view.checking; visible: running; implicitWidth: 32; implicitHeight: 32 }
                Button {
                    text: view.checking ? qsTr("Checking…") : qsTr("Connect")
                    highlighted: true
                    enabled: !view.checking && tokenField.text.trim() !== ""
                    Material.roundedScale: theme.radius
                    contentItem: Label {
                        text: parent.text
                        color: theme.accentForeground
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitWidth: 110
                        implicitHeight: 38
                        radius: Math.min(6, theme.radius)
                        color: theme.accent
                        opacity: parent.enabled ? (parent.down ? 0.8 : 1) : 0.35
                    }
                    onClicked: view.submit()
                }
            }
        }
    }
}
