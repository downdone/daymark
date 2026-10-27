pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls

DS.Menu {
    id: root
    property var options: []
    property var selectedIds: []
    signal selected(string id, bool checked)
    signal createRequested()
    readonly property var hostWindow: parent ? parent.Window.window : null
    width: Math.min(implicitWidth, 320 * Theme.scale, hostWindow ? hostWindow.width - 16 : 640)
    height: Math.min(implicitHeight, hostWindow ? hostWindow.height - 16 : 480)
    onAboutToShow: {
        root.updateGroups();
        if (!parent || !hostWindow)
            return;
        const at = parent.mapToItem(hostWindow.contentItem, 0, 0);
        x = Math.max(8 - at.x, Math.min(0, hostWindow.width - at.x - width - 8));
        y = hostWindow.height - at.y - parent.height - 8 >= height ? parent.height + 4
            : at.y - 8 >= height ? -height - 4
            : Math.max(8 - at.y, hostWindow.height - at.y - height - 8);
    }
    Instantiator {
        model: root.options
        delegate: MenuItem {
            required property var modelData
            objectName: root.objectName + "-option-" + modelData.id
            text: modelData.title
            checkable: true
            checked: root.selectedIds.indexOf(modelData.id) >= 0
            onTriggered: root.selected(modelData.id, root.selectedIds.indexOf(modelData.id) < 0)
        }
        onObjectAdded: function(index, object) {root.insertItem(index, object)}
        onObjectRemoved: function(index, object) {root.removeItem(object)}
    }
    MenuItem {text: qsTr("暂无标签");enabled: false;visible: root.options.length === 0}
    DS.MenuSeparator {objectName: root.objectName + "-separator"}
    MenuItem {objectName: root.objectName + "-create";text: qsTr("新建标签…");onTriggered: root.createRequested()}
}
