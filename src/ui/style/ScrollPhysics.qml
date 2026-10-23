import QtQuick
QtObject {
    id: policy
    objectName: "daymarkScrollPhysics"
    required property Flickable target
    readonly property bool overflowing: !!target && (target.contentHeight>target.height+1 || target.contentWidth>target.width+1)
    property Binding bounds: Binding {restoreMode: Binding.RestoreNone;target: policy.target;property: "boundsBehavior";value: Flickable.StopAtBounds}
    property Binding velocity: Binding {restoreMode: Binding.RestoreNone;target: policy.target;property: "maximumFlickVelocity";value: 1200}
    property Binding deceleration: Binding {restoreMode: Binding.RestoreNone;target: policy.target;property: "flickDeceleration";value: 6000}
    property Binding interaction: Binding {restoreMode: Binding.RestoreNone;target: policy.target;property: "interactive";value: policy.overflowing}
    property Binding keyboard: Binding {restoreMode: Binding.RestoreNone;target: policy.target;property: "keyNavigationEnabled";when: policy.target instanceof ListView;value: true}
}
