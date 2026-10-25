pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Flow {
    id: root
    property int currentIndex: 0
    readonly property var keys: ["neutral","blue","green","orange","purple"]
    readonly property var names: [qsTr("中性"),qsTr("蓝"),qsTr("绿"),qsTr("橙"),qsTr("紫")]
    signal activated(int index)
    Layout.fillWidth: true
    spacing: 4
    function choose(index) {
        const next=Math.max(0,Math.min(keys.length-1,index))
        currentIndex=next;activated(next)
        const item=swatches.itemAt(next) as ToolButton
        if(item)item.forceActiveFocus()
    }
    function activateForVerification(index) {
        const item=swatches.itemAt(index) as ToolButton
        if(!item || !item.visible || item.width<=0 || item.height<=0)return false
        item.clicked();return true
    }
    Repeater {
        id: swatches
        model: root.keys
        ToolButton {
            id: swatch
            objectName: "catalogColor-"+index
            required property int index
            required property string modelData
            width: Math.max(32,24*Theme.scale);height: width
            Accessible.name: root.names[index]
            Accessible.role: Accessible.RadioButton
            Accessible.checkable: true;Accessible.checked: root.currentIndex===index
            onClicked: root.choose(index)
            Keys.onLeftPressed: root.choose(index-1)
            Keys.onRightPressed: root.choose(index+1)
            contentItem: Item {
                Rectangle {anchors.centerIn: parent;width: Math.min(22*Theme.scale,swatch.width-10);height: width;radius: width/2;color: Theme.catalogColor(swatch.modelData)
                    Rectangle {anchors.centerIn: parent;width: parent.width+6;height: width;radius: width/2;color: "transparent";border.width: 2;border.color: Theme.catalogColor(swatch.modelData);visible: root.currentIndex===swatch.index}
                }
            }
        }
    }
}
