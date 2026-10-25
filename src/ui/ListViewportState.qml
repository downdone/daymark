pragma ComponentBehavior: Bound
import QtQuick

// UI-only identity/scroll restoration. QueryService remains the source of membership.
QtObject {
    id: root
    required property ListView list
    required property TaskWorkspace workspace
    property var positions: ({})
    property string previousSearch: ""
    property string searchScope: ""
    property var searchPosition: null
    property bool restoreSearch: false
    function snapshot() {
        const item = list.currentItem as TaskRow;
        return {
            y: list.contentY,
            id: item ? item.taskId : "",
            index: list.currentIndex,
            count: list.count,
            offset: item ? item.y - list.contentY : 0
        };
    }
    function capture(scope) {
        positions[scope] = snapshot();
    }
    function restorePosition(saved) {
        if (!saved) {
            list.positionViewAtBeginning();
            return;
        }
        const index = workspace.taskIndex(saved.id);
        list.currentIndex = index >= 0 ? index : Math.min(saved.index, list.count - 1);
        if (index === saved.index && list.count === saved.count)
            list.contentY = bounded(saved.y);
        else if (index >= 0) {
            list.positionViewAtIndex(index, ListView.Beginning);
            list.contentY = bounded(list.contentY - saved.offset);
        } else
            list.contentY = bounded(saved.y);
    }
    function bounded(y) {
        return Math.max(list.originY, Math.min(y, list.originY + Math.max(0, list.contentHeight - list.height)));
    }
    function restore(scope) {
        restorePosition(positions[scope]);
        list.forceActiveFocus();
    }
    function searchChanged(text) {
        if (text && !previousSearch) {
            searchScope = workspace.scope;
            searchPosition = snapshot();
            restoreSearch = false;
        }
        if (!text && previousSearch)
            restoreSearch = true;
        previousSearch = text;
    }
    property Connections modelChanges: Connections {
        target: root.workspace.tasks
        function onCountChanged() {
            if (!root.restoreSearch)
                return;
            if (root.workspace.scope !== root.searchScope) {
                root.restoreSearch = false;
                return;
            }
            if (root.previousSearch.length > 0)
                return;
            root.restoreSearch = false;
            Qt.callLater(function () {
                root.restorePosition(root.searchPosition);
            });
        }
    }
}
