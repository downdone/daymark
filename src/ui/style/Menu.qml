import QtQuick
import QtQuick.Templates as T
T.Menu {
    id: control
    popupType: T.Popup.Item
    // Conditional commands must not leave leading, trailing or doubled rules.
    function updateGroups() {
        let hasItem=false,pending=null
        const shown=[]
        for(let i=0;i<count;++i){
            const item=itemAt(i),separator=item as MenuSeparator
            if(separator){if(separator.groupEnabled && !pending)pending=separator}
            else if(item && item.visible){if(hasItem && pending)shown.push(pending);hasItem=true;pending=null}
        }
        for(let i=0;i<count;++i){const separator=itemAt(i) as MenuSeparator;if(separator)separator.groupHasNeighbors=shown.indexOf(separator)>=0}
    }
    function verifyGrouping() {
        let hasItem=false,previousRule=false
        for(let i=0;i<count;++i){const item=itemAt(i);if(!item || !item.visible)continue;if(item instanceof MenuSeparator){if(!hasItem || previousRule)return "orphan or doubled separator";previousRule=true}else {hasItem=true;previousRule=false;const command=item as MenuItem;if(command && command.verifyColumns()!=="PASS")return command.verifyColumns()}}
        return previousRule ? "trailing separator" : "PASS"
    }
    // A menu shared by Mini and the expanded widget must acquire a fresh anchor.
    property Item anchorItem: null
    readonly property var anchorWindow: anchorItem ? anchorItem.Window.window : null
    readonly property var menuWindow: contentItem ? contentItem.Window.window : null
    function anchorPosition(source) {
        const host=source.Window.window,screen=host ? host.screen : null
        const edge=source.mapToGlobal(source.width,source.height)
        const left=screen ? Math.max(screen.virtualX+6,Math.min(edge.x-width,screen.virtualX+screen.width-width-6)) : edge.x-width
        return source.mapFromGlobal(left,edge.y+4)
    }
    function popupAt(source) {
        anchorItem=source
        recordHostGeometry()
        const at=anchorPosition(source)
        popup(source,at.x,at.y)
    }
    function refreshAnchor() {
        if(!visible || !anchorItem)return
        const at=anchorPosition(anchorItem)
        x=at.x;y=at.y
    }
    onWidthChanged: if(visible && anchorItem)Qt.callLater(refreshAnchor)
    onHeightChanged: if(visible && anchorItem)Qt.callLater(refreshAnchor)
    onOpened: {recordHostGeometry();updateGroups();refreshAnchor()}
    opacity: 1
    Binding {target:control.menuWindow;property:"color";value:DesignTokens.surface;when:control.popupType===T.Popup.Window && !!control.menuWindow && !!control.anchorWindow && control.menuWindow!==control.anchorWindow;restoreMode:Binding.RestoreBindingOrValue}
    Binding {target:control.menuWindow;property:"opacity";value:1;when:control.popupType===T.Popup.Window && !!control.menuWindow && !!control.anchorWindow && control.menuWindow!==control.anchorWindow;restoreMode:Binding.RestoreBindingOrValue}
    property rect hostGeometry: Qt.rect(0,0,0,0)
    function recordHostGeometry() {
        const host=anchorWindow
        if(host)hostGeometry=Qt.rect(host.x,host.y,host.width,host.height)
    }
    function closeForHostGeometryChange() {
        const host=anchorWindow
        // Native resize acknowledgements may repeat an already applied value.
        if(visible && host && (host.x!==hostGeometry.x || host.y!==hostGeometry.y || host.width!==hostGeometry.width || host.height!==hostGeometry.height))close()
    }
    Connections {
        target:control.anchorWindow
        function onXChanged(){control.closeForHostGeometryChange()}
        function onYChanged(){control.closeForHostGeometryChange()}
        function onWidthChanged(){control.closeForHostGeometryChange()}
        function onHeightChanged(){control.closeForHostGeometryChange()}
    }
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
    onAboutToShow: {updateGroups();motionSurface1.forceLayout();Qt.callLater(motionSurface1.forceLayout)}
    onEntriesHeightChanged: if(visible){Qt.callLater(updateGroups);Qt.callLater(motionSurface1.forceLayout)}
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
    background: Rectangle {color: DesignTokens.surface;radius: DesignTokens.menuRadius;border.color: DesignTokens.border}
}
