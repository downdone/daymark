pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: picker
    required property var model
    property int currentIndex: 0
    property int textPixelSize: Theme.bodySize
    property bool selectionIsExternal: false
    signal activated(int index)
    Layout.fillWidth: true
    implicitHeight: Math.max(Theme.controlHeight,row.implicitHeight+4)
    padding: 2
    background: Rectangle {radius: Theme.menuRadius;color: Theme.hover}
    function select(index) {
        const next=Math.max(0,Math.min(model.length-1,index))
        if(selectionIsExternal && next===currentIndex)return
        if(!selectionIsExternal)currentIndex=next
        activated(next)
    }
    function activateForVerification(index) {
        const button=segments.itemAt(index) as Button
        if(!button)return false
        button.clicked()
        return true
    }
    contentItem: RowLayout {
        id: row
        spacing: 2
        Repeater {
            id: segments
            model: picker.model
            Button {
                id: segment
                objectName: picker.objectName+"-segment-"+index
                required property string modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: Math.max(28,Theme.bodySize+10)
                text: modelData
                checked: picker.currentIndex===index
                font.pixelSize: picker.textPixelSize
                font.weight: checked ? Font.DemiBold : Font.Normal
                palette.buttonText: checked ? Theme.text : Theme.secondary
                Accessible.role: Accessible.RadioButton
                Accessible.checkable: true
                Accessible.checked: checked
                onClicked: picker.select(index)
                Keys.onLeftPressed: {picker.select(index-1);const item=segments.itemAt(Math.max(0,index-1)) as Button;if(item)item.forceActiveFocus()}
                Keys.onRightPressed: {picker.select(index+1);const item=segments.itemAt(Math.min(picker.model.length-1,index+1)) as Button;if(item)item.forceActiveFocus()}
                background: Rectangle {radius: Theme.controlRadius;color: segment.down ? Theme.pressed : segment.checked ? Theme.content : segment.hovered ? Theme.selected : "transparent";border.width: segment.visualFocus || Theme.highContrast && segment.checked ? 1 : 0;border.color: segment.visualFocus ? Theme.accent : Theme.secondary}
            }
        }
    }
}
