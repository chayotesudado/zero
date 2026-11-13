pragma ComponentBehavior: Bound
import QtQuick
import Zero

// Six dots, shown on hover left of a task or a section's heading, saying
// it can be dragged.
Grid {
    columns: 2
    spacing: 2
    opacity: 0.7
    Repeater {
        model: 6
        Rectangle { width: 2; height: 2; radius: 1; color: Theme.dimForeground }
    }
}
