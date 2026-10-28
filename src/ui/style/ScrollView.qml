import QtQuick
import QtQuick.Templates as T
T.ScrollView {
    id: control
    objectName: "daymarkScrollView"
    // Vertical surfaces take their width from the host; deriving it back from
    // availableWidth would create a dialog -> content -> dialog sizing cycle.
    implicitWidth: 240
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    contentWidth: availableWidth
    rightPadding: DesignTokens.scrollGutter
    clip: true
    wheelEnabled: physics.overflowing
    property ScrollPhysics scrolling: ScrollPhysics {id: physics;target: control.contentItem as Flickable}
    ScrollBar.vertical: ScrollBar {parent: control;x: control.width-width;y: control.topPadding;height: control.availableHeight;policy: ScrollBar.AsNeeded}
    ScrollBar.horizontal: ScrollBar {policy: ScrollBar.AlwaysOff}
}
