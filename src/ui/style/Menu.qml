import QtQuick
import QtQuick.Templates as T
T.Menu {
    id: control
    popupType: T.Popup.Item
    property bool registeredHintBlock: false
    onVisibleChanged: {
        if(visible && !registeredHintBlock){registeredHintBlock=true;++DesignTokens.activeMenus}
        else if(!visible && registeredHintBlock){registeredHintBlock=false;DesignTokens.activeMenus=Math.max(0,DesignTokens.activeMenus-1)}
    }
    Component.onDestruction: if(registeredHintBlock)DesignTokens.activeMenus=Math.max(0,DesignTokens.activeMenus-1)
    readonly property real entriesHeight: {
        let total=0
        for(let i=0;i<count;i++) {const item=itemAt(i);if(item && item.visible)total+=item.implicitHeight}
        return total
    }
    onAboutToShow: {motionSurface1.forceLayout();Qt.callLater(motionSurface1.forceLayout)}
    onEntriesHeightChanged: if(visible)Qt.callLater(motionSurface1.forceLayout)
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
    implicitHeight: Math.min(480,entriesHeight+topPadding+bottomPadding)
    padding: 6
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.menuFontSize
    font.weight: Font.Normal
    delegate: MenuItem {}
    contentItem: ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
        id: motionSurface1
        ScrollPhysics {target: motionSurface1}
        implicitHeight: control.entriesHeight;model: control.contentModel;currentIndex: control.currentIndex;interactive: contentHeight>height;clip: true;boundsBehavior: Flickable.StopAtBounds}
    background: Rectangle {color: DesignTokens.surface;radius: DesignTokens.panelRadius;border.color: DesignTokens.border}
}
