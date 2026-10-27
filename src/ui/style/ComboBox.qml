pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
T.ComboBox {
    id: control
    property var separatorBefore: []
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
        y: control.height+4
        width: control.width
        // Bootstrap a nonzero viewport without relying on offscreen cache rows.
        implicitHeight: Math.min(320,Math.max(contentItem.implicitHeight,Math.min(control.count,8)*DesignTokens.controlHeight)+12)
        padding: 6
        contentItem: ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
            id: motionSurface1
            ScrollPhysics {target: motionSurface1}
            implicitHeight: contentHeight
            clip: true
            // Closed popups must not incubate rows while form models refresh.
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
        }
        background: Rectangle {radius: DesignTokens.menuRadius;color: DesignTokens.surface;border.color: DesignTokens.border}
    }
}
