pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: picker
    required property var model
    property int currentIndex: 0
    property bool selectionIsExternal: false
    signal activated(int index)
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight+6
    padding: 3
    background: Rectangle {radius: Theme.rowRadius;color: Theme.hover}
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
                text: modelData
                checked: picker.currentIndex===index
                font.weight: checked ? Font.DemiBold : Font.Normal
                palette.buttonText: checked ? Theme.accent : Theme.secondary
                Accessible.role: Accessible.RadioButton
                Accessible.checkable: true
                Accessible.checked: checked
                onClicked: picker.select(index)
                Keys.onLeftPressed: {picker.select(index-1);const item=segments.itemAt(Math.max(0,index-1)) as Button;if(item)item.forceActiveFocus()}
                Keys.onRightPressed: {picker.select(index+1);const item=segments.itemAt(Math.min(picker.model.length-1,index+1)) as Button;if(item)item.forceActiveFocus()}
                background: Rectangle {radius: 6;color: segment.checked ? Theme.content : segment.hovered ? Theme.selected : "transparent";border.width: segment.visualFocus ? 2 : 0;border.color: Theme.accent}
            }
        }
    }
}
