pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
T.ComboBox {
    id: control
    property var separatorBefore: []
    property bool withinHostWindow: false
    readonly property var hostWindow: Window.window
    implicitWidth: Math.max(140,implicitContentWidth+leftPadding+rightPadding)
    implicitHeight: Math.max(DesignTokens.controlHeight,implicitContentHeight+12)
    leftPadding: 10
    rightPadding: 32
    font.pixelSize: DesignTokens.body
    font.family: DesignTokens.fontFamily
    hoverEnabled: true
    wheelEnabled: false
    contentItem: TextInput {
        enabled: control.editable
        text: control.editable ? control.editText : control.displayText
        font: control.font
        color: control.enabled ? DesignTokens.text : DesignTokens.disabledText
        selectionColor: DesignTokens.accent
        selectedTextColor: DesignTokens.accentText
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        selectByMouse: control.selectTextByMouse
        verticalAlignment: TextInput.AlignVCenter
        clip: true
    }
    indicator: Glyph {
        x: control.width-width-10
        y: (control.height-height)/2
        width:Math.min(24,14*DesignTokens.scale);height:width
        name:"down";ink:control.enabled ? DesignTokens.secondary : DesignTokens.disabledText
    }
    background: FieldSurface {focused: control.visualFocus;hovered: control.hovered || control.down;enabledField: control.enabled}
    delegate: MenuItem {
        required property int index
        required property var model
        objectName:control.objectName+"-option-"+index
        groupStart: index>0 && control.separatorBefore.indexOf(index)>=0
        width: control.width
        text: control.textRole ? model[control.textRole] : model.modelData
        checkable: true
        checked: control.currentIndex===index
        highlighted: control.highlightedIndex===index
    }
    popup: T.Popup {
        id: optionsPopup
        // Keep nested fields in the parent's input surface. Other desktop
        // dropdowns can use a separate window to reach the screen work area.
        popupType: control.withinHostWindow || Qt.platform.pluginName.startsWith("wayland") || Qt.platform.pluginName === "offscreen"
            ? T.Popup.Item : T.Popup.Window
        margins: 8
        padding: 6
        readonly property int anchorGap: 4
        property rect workArea
        readonly property real heightLimit: Math.max(0, (workArea.height || (control.hostWindow ? control.hostWindow.height : 0)) - 2 * margins)
        implicitWidth: workArea.width > 0 ? Math.min(control.width, workArea.width - 2 * margins) : control.width
        // Reserve the initial viewport before the native popup opens. Use the
        // measured rows as soon as the ListView starts laying out delegates.
        implicitHeight: Math.min(heightLimit, (optionsList.contentHeight > 0 ? optionsList.contentHeight
            : control.count * DesignTokens.menuRowHeight) + topPadding + bottomPadding)
        function placeAtAnchor() {
            if (!parent || workArea.height <= 0) {
                x = 0
                y = control.height + anchorGap
                return
            }
            const top = control.mapToGlobal(0, 0)
            const bottom = control.mapToGlobal(0, control.height)
            const minY = workArea.y + margins
            const maxY = workArea.y + workArea.height - margins
            const below = bottom.y + anchorGap
            const above = top.y - anchorGap - implicitHeight
            let targetY = below
            if (below + implicitHeight > maxY)
                targetY = above >= minY ? above : maxY - implicitHeight
            targetY = Math.max(minY, targetY)
            const minX = workArea.x + margins
            const maxX = workArea.x + workArea.width - margins - implicitWidth
            const at = parent.mapFromGlobal(Math.max(minX, Math.min(top.x, maxX)), targetY)
            x = at.x
            y = at.y
        }
        function refreshWorkArea() {
            if (popupType === T.Popup.Window) {
                workArea = ScreenWorkArea.forItem(control)
            } else if (control.hostWindow) {
                const viewport = control.hostWindow.contentItem
                const origin = viewport.mapToGlobal(0, 0)
                workArea = Qt.rect(origin.x, origin.y, viewport.width, viewport.height)
            }
            placeAtAnchor()
        }
        onAboutToShow: refreshWorkArea()
        onImplicitHeightChanged: if (visible) placeAtAnchor()
        onImplicitWidthChanged: if (visible) placeAtAnchor()
        Connections {
            target: optionsPopup.visible ? control.hostWindow : null
            function onXChanged() { optionsPopup.refreshWorkArea() }
            function onYChanged() { optionsPopup.refreshWorkArea() }
            function onWidthChanged() { optionsPopup.refreshWorkArea() }
            function onHeightChanged() { optionsPopup.refreshWorkArea() }
            function onScreenChanged() { optionsPopup.refreshWorkArea() }
        }
        contentItem: ListView {
            id: optionsList
            // Avoid background row incubation across model resets on Qt 6.10.2.
            cacheBuffer: 0
            ScrollPhysics {target: optionsList}
            clip: true
            // Closed popups must not incubate rows while form models refresh.
            model: optionsPopup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
        }
        background: Rectangle {radius: DesignTokens.menuRadius;color: DesignTokens.surface;border.color: DesignTokens.border}
    }
}
