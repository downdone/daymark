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
    property point sourcePoint: Qt.point(-1, -1)
    property Item sourceRow: null
    property string sourceId: ""
    property bool sourceFromList: true
    property bool hasSource: false
    property bool sourceLocated: false
    property bool positionedManually: false
}
