import QtQuick
import QtQuick.Templates as T
T.MenuSeparator {
    property bool groupEnabled: true
    property bool groupHasNeighbors: true
    visible: groupEnabled && groupHasNeighbors
    implicitWidth: 100
    implicitHeight: 9
    height: visible ? implicitHeight : 0
    verticalPadding: 4
    contentItem: Rectangle {implicitHeight: 1;color: DesignTokens.border}
}
