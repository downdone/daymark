pragma ComponentBehavior: Bound
import QtQuick

// Pointer recognition only. The owner decides the preview and commits on release.
MouseArea {
    id: root
    anchors.fill: parent
    required property var entry
    required property int dragDistance
    property bool started: false
    property point origin: Qt.point(0, 0)
    signal dragStarted(var entry, Item source, point origin)
    signal dragMoved(Item source, point position)
    signal dragEnded(bool commit)
    signal activated
    cursorShape: started && pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
    onPressed: function (mouse) {
        started = false;
        origin = Qt.point(mouse.x, mouse.y);
    }
    onPositionChanged: function (mouse) {
        if (!pressed)
            return;
        if (!started && Math.abs(mouse.x - origin.x) + Math.abs(mouse.y - origin.y) >= dragDistance) {
            started = true;
            dragStarted(entry, root, origin);
        }
        if (started)
            dragMoved(root, Qt.point(mouse.x, mouse.y));
    }
    onReleased: {
        if (started)
            dragEnded(true);
        else
            activated();
    }
    onCanceled: dragEnded(false)
}
