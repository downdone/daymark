import QtQuick
QtObject {
    id: policy
    objectName: "daymarkScrollPhysics"
    required property Flickable target
    property bool attached: true
    property bool keyboardNavigationEnabled: true
    readonly property bool overflowing: {
        if(!policy.attached)return false;
        const view=policy.target;
        return !!view && (view.contentHeight>view.height+1 || view.contentWidth>view.width+1);
    }
    function detach() {policy.attached=false;policy.target=null}
    Component.onDestruction: policy.detach()
    property Binding bounds: Binding {when:policy.attached;restoreMode: Binding.RestoreNone;target: policy.target;property: "boundsBehavior";value: Flickable.StopAtBounds}
    property Binding velocity: Binding {when:policy.attached;restoreMode: Binding.RestoreNone;target: policy.target;property: "maximumFlickVelocity";value: 1200}
    property Binding deceleration: Binding {when:policy.attached;restoreMode: Binding.RestoreNone;target: policy.target;property: "flickDeceleration";value: 6000}
    property Binding interaction: Binding {when:policy.attached;restoreMode: Binding.RestoreNone;target: policy.target;property: "interactive";value: policy.overflowing}
    property Binding keyboard: Binding {restoreMode: Binding.RestoreNone;target: policy.target;property: "keyNavigationEnabled";when: policy.attached && policy.target instanceof ListView;value: policy.keyboardNavigationEnabled}
}
