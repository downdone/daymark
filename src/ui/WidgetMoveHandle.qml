pragma ComponentBehavior: Bound
import QtQuick

DragHandler {
    id:drag
    required property WidgetViewModel controller
    target:null
    enabled:!controller.locked && !controller.clickThrough
    acceptedButtons:Qt.LeftButton
    cursorShape:active?Qt.ClosedHandCursor:Qt.OpenHandCursor
    onActiveChanged:{if(active)controller.beginPointerMove(parent.mapToGlobal(centroid.pressPosition));else controller.finishMove()}
    onActiveTranslationChanged:if(active)controller.movePointer(parent.mapToGlobal(centroid.position))
}
