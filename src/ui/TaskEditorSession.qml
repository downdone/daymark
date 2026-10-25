import QtQuick

QtObject {
    property string taskId: ""
    property real version: 0
    property bool dirty: false
    property bool saving: false
    property bool saveError: false
    property var pendingAction: null
    property string savingTaskId: ""
    property bool expanded: false
    property point anchor: Qt.point(0, 0)
}
