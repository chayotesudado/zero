pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import Zero

// Line icons for the fixed lists: an in-tray for the Inbox, a calendar with
// today's date in it for Today, and a calendar of days for Upcoming. Paths
// are Lucide's (ISC licence), drawn on its 24-unit grid.
Item {
    id: icon
    property string kind: "inbox"
    property color color: Theme.foreground
    property int day: new Date().getDate()

    implicitWidth: 18
    implicitHeight: 18

    readonly property string calendar: "M5 4h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z M16 2v4 M8 2v4 M3 10h18"
    readonly property var paths: ({
        inbox: "M22 12h-6l-2 3h-4l-2-3H2 M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z",
        today: calendar,
        upcoming: calendar + " M8 14h.01 M12 14h.01 M16 14h.01 M8 18h.01 M12 18h.01 M16 18h.01"
    })

    Shape {
        width: 24
        height: 24
        scale: icon.width / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: icon.color
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.kind] || "" }
        }
    }

    // Today's date sits in the calendar's body, below the binding bar.
    Text {
        visible: icon.kind === "today"
        x: icon.width * 3 / 24
        y: icon.height * 10 / 24
        width: icon.width * 18 / 24
        height: icon.height * 12 / 24
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: icon.day.toLocaleString(Translator.locale, 'f', 0)
        color: icon.color
        font.pixelSize: Math.round(icon.height * 0.42)
        font.bold: true
    }

    // Rolls the date over at midnight while the app stays open.
    Timer {
        running: icon.kind === "today"
        repeat: true
        interval: 60000
        onTriggered: icon.day = new Date().getDate()
    }
}
