pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    required property var model
    property int currentIndex: 0
    signal activated(int index)
    Accessible.role: Accessible.PageTabList
    implicitWidth: 248*Theme.scale
    implicitHeight: Math.max(32,Theme.bodySize+14)
    padding: 3
    background: Rectangle {radius:Theme.menuRadius;color:Theme.hover}
    function select(index) {
        const next=Math.max(0,Math.min(model.length-1,index));
        if(next!==currentIndex)activated(next);
    }
    function focusTab(index) {
        const tab=tabs.itemAt(Math.max(0,Math.min(model.length-1,index))) as Button;
        if(tab)tab.forceActiveFocus();
    }
    onCurrentIndexChanged: {
        for(let i=0;i<model.length;i++){
            const tab=tabs.itemAt(i) as Button;
            if(tab && tab.activeFocus){focusTab(currentIndex);break;}
        }
    }
    function activateForVerification(index) {
        const tab=tabs.itemAt(index) as Button;
        if(!tab)return false;
        tab.clicked();return true;
    }
    contentItem: RowLayout {
        spacing: 2
        Repeater {
            id: tabs
            model: root.model
            Button {
                id: tab
                required property int index
                required property string modelData
                objectName: root.objectName+"-segment-"+index
                Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredWidth:1
                text:modelData
                checked:root.currentIndex===index
                focusPolicy:checked ? Qt.StrongFocus : Qt.ClickFocus
                font.pixelSize:Theme.bodySize
                font.weight:checked ? Font.DemiBold : Font.Normal
                Accessible.role:Accessible.PageTab
                Accessible.selected:checked
                contentItem:Text {
                    text:tab.text;font:tab.font
                    color:!tab.enabled ? Theme.disabledText : tab.checked ? Theme.text : Theme.secondary
                    horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight
                }
                background:Rectangle {
                    radius:Theme.controlRadius
                    color:tab.down ? Theme.pressed : tab.checked ? Theme.content : tab.hovered ? Theme.selected : "transparent"
                    border.width:tab.visualFocus || Theme.highContrast && tab.checked ? 1 : 0
                    border.color:tab.visualFocus ? Theme.accent : Theme.secondary
                }
                onClicked:root.select(index)
                Keys.onLeftPressed:{root.select(index-1);root.focusTab(root.currentIndex)}
                Keys.onRightPressed:{root.select(index+1);root.focusTab(root.currentIndex)}
                Keys.onPressed:function(event) {
                    if(event.key===Qt.Key_Home || event.key===Qt.Key_End){root.select(event.key===Qt.Key_Home ? 0 : root.model.length-1);root.focusTab(root.currentIndex);event.accepted=true}
                }
            }
        }
    }
}
