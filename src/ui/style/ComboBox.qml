pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
T.ComboBox {
    id: control
    implicitWidth: Math.max(140,implicitContentWidth+leftPadding+rightPadding)
    implicitHeight: Math.max(DesignTokens.controlHeight,implicitContentHeight+12)
    leftPadding: 10
    rightPadding: 32
    font.pixelSize: DesignTokens.body
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
    indicator: Canvas {
        x: control.width-width-10
        y: (control.height-height)/2
        width: 14;height: 14
        property color ink: control.enabled ? DesignTokens.secondary : DesignTokens.disabledText
        onInkChanged: requestPaint()
        onPaint: {const c=getContext("2d");c.reset();c.strokeStyle=ink;c.lineWidth=1.7;c.lineCap="round";c.lineJoin="round";c.beginPath();c.moveTo(3,5);c.lineTo(7,9);c.lineTo(11,5);c.stroke()}
    }
    background: Rectangle {radius: DesignTokens.radius;color: control.down ? DesignTokens.selected : control.hovered ? DesignTokens.hover : DesignTokens.surface;border.width: control.visualFocus ? 2 : 1;border.color: control.visualFocus ? DesignTokens.accent : control.hovered ? DesignTokens.secondary : DesignTokens.outline}
    delegate: MenuItem {
        required property int index
        required property var model
        width: control.width
        text: control.textRole ? model[control.textRole] : model.modelData
        highlighted: control.highlightedIndex===index
    }
    popup: T.Popup {
        y: control.height+4
        width: control.width
        implicitHeight: Math.min(320,contentItem.implicitHeight+12)
        padding: 6
        contentItem: ListView {
            id: motionSurface1
            ScrollPhysics {target: motionSurface1}
            implicitHeight: contentHeight
            clip: true
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
        }
        background: Rectangle {radius: DesignTokens.radius;color: DesignTokens.surface;border.color: DesignTokens.border}
    }
}
