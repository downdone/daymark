import QtQuick
import QtQuick.Templates as T
T.Menu {
    id: control
    popupType: T.Popup.Window
    margins: 6
    implicitWidth: {
        let widest=180*DesignTokens.scale
        for(let i=0;i<count;i++) {
            const item=itemAt(i)
            if(item && item.visible)widest=Math.max(widest,item.implicitWidth+leftPadding+rightPadding)
        }
        return widest
    }
    width: implicitWidth
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    padding: 6
    font.pixelSize: DesignTokens.body
    delegate: MenuItem {}
    contentItem: ListView {implicitHeight: contentHeight;model: control.contentModel;currentIndex: control.currentIndex;interactive: contentHeight>height;clip: true;boundsBehavior: Flickable.StopAtBounds}
    background: Rectangle {color: DesignTokens.surface;radius: DesignTokens.radius;border.color: DesignTokens.border}
}
