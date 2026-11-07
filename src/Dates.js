.pragma library

// Due dates as Todoist users read them: Today, Tomorrow, a weekday this week,
// otherwise a short date, always in the Omarchy language's locale.

function parseDay(iso) {
    var p = String(iso).split("-")
    return new Date(+p[0], +p[1] - 1, +p[2])
}

function timeLabel(task, locale) {
    if (!task.dueTime)
        return ""
    var p = task.dueTime.split(":")
    var d = new Date(2000, 0, 1, +p[0], +p[1])
    return d.toLocaleTimeString(locale, locale.timeFormat(1 /* Locale.ShortFormat */))
}

function dayLabel(offset, iso, locale) {
    if (offset === 0) return qsTr("Today")
    if (offset === 1) return qsTr("Tomorrow")
    if (offset === -1) return qsTr("Yesterday")
    var d = parseDay(iso)
    if (offset > 1 && offset < 7)
        return d.toLocaleDateString(locale, "dddd")
    return d.toLocaleDateString(locale, offset < -180 || offset > 180 ? "d MMM yyyy" : "d MMM")
}

function dueLabel(task, locale) {
    if (!task.hasDue)
        return ""
    var day = dayLabel(task.dayOffset, task.dueDate, locale)
    var time = timeLabel(task, locale)
    return time ? qsTr("%1 %2").arg(day).arg(time) : day
}

// Headings for the grouped Today and Upcoming lists.
function sectionLabel(group, today, locale) {
    if (group === "overdue") return qsTr("Overdue")
    if (!group) return qsTr("No date")
    var d = parseDay(group)
    var offset = Math.round((d - parseDay(today)) / 86400000)
    var date = d.toLocaleDateString(locale, "ddd d MMM")
    if (offset === 0) return qsTr("Today · %1").arg(date)
    if (offset === 1) return qsTr("Tomorrow · %1").arg(date)
    return d.toLocaleDateString(locale, "dddd · d MMM")
}

function isoToday() {
    var d = new Date()
    function two(n) { return (n < 10 ? "0" : "") + n }
    return d.getFullYear() + "-" + two(d.getMonth() + 1) + "-" + two(d.getDate())
}

// Todoist's priority colors (p1 red, p2 orange, p3 blue) in the theme's own
// shades of them; p4 is the plain dim foreground.
function priorityColor(priority, theme) {
    return priority === 1 ? theme.red : priority === 2 ? theme.orange : priority === 3 ? theme.blue : theme.dimForeground
}
