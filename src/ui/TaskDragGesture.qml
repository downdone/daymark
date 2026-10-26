pragma ComponentBehavior: Bound
import QtQuick

// Pointer recognition only. The owner decides the preview and commits on release.
MouseArea {
    id: root
    anchors.fill: parent
    preventStealing: true
    required property var entry
    required property int dragDistance
    property bool started: false
    property point origin: Qt.point(0, 0)
    signal dragStarted(var entry, Item source, point origin)
    signal dragMoved(Item source, point position)
    signal dragEnded(bool commit)
    property bool doubleTap: false
    property bool contextGesture: false
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    signal contextRequested(point position)
    signal doubleActivated
    signal activated
    cursorShape: started && pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
    onPressed: function (mouse) {
        started = false;doubleTap=false;
        contextGesture=mouse.button===Qt.RightButton || (Qt.platform.os==="osx" && (mouse.modifiers & Qt.MetaModifier));
        if(contextGesture){contextRequested(Qt.point(mouse.x,mouse.y));return}
        origin = Qt.point(mouse.x, mouse.y);
    }
    onPositionChanged: function (mouse) {
        if (!pressed || contextGesture)
            return;
        if (!started && Math.abs(mouse.x - origin.x) + Math.abs(mouse.y - origin.y) >= dragDistance) {
            started = true;
            dragStarted(entry, root, origin);
        }
        if (started)
            dragMoved(root, Qt.point(mouse.x, mouse.y));
    }
    onDoubleClicked: {if(!contextGesture){doubleTap=true;doubleActivated()}}
    onReleased: {
        if(contextGesture)return
        if (started)
            dragEnded(true);
        else if(!doubleTap)
            activated();
    }
    onCanceled: dragEnded(false)
}
