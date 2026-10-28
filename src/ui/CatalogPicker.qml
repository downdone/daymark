pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS
Popup {
    id: root
    property string heading: ""
    property var options: []
    property var selectedIds: []
    property bool multiple: false
    property real heightLimit: 440
    readonly property var hostWindow: parent ? parent.Window.window : null
    signal selected(string kind,string id,bool checked)
    signal createRequested(string kind)
    readonly property var matches: options.filter(item=>!search.text || (item.path || item.title).toLocaleLowerCase().indexOf(search.text.toLocaleLowerCase())>=0)
    width: Math.min(320*DS.DesignTokens.scale,hostWindow ? hostWindow.width-24 : 640)
    height: Math.min(heightLimit,Math.max(220,140+Math.min(8,options.length)*32))
    padding: 12;margins: 8;focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {search.clear();choices.positionViewAtBeginning();search.forceActiveFocus()}
    onAboutToShow: {
        const at=parent.mapToItem(null,0,0)
        const windowHeight=hostWindow ? hostWindow.height : 720
        x=Math.max(8-at.x,Math.min(0,(hostWindow ? hostWindow.width : 1120)-at.x-width-8))
        y=windowHeight-at.y-parent.height-12>=height ? parent.height+4 : at.y-12>=height ? -height-4 : Math.max(8-at.y,windowHeight-at.y-height-8)
    }
    background: Rectangle {color: DS.DesignTokens.surface;radius: DS.DesignTokens.panelRadius;border.color: DS.DesignTokens.border}
    contentItem: ColumnLayout {
        spacing: 8
        Label {text: root.heading;font.weight: Font.DemiBold}
        TextField {id: search;Layout.fillWidth: true;placeholderText: qsTr("搜索…");Accessible.name: qsTr("搜索%1").arg(root.heading)}
        ListView {
            id: choices
            Layout.fillWidth: true;Layout.fillHeight: true
            cacheBuffer: 0;clip: true;boundsBehavior: Flickable.StopAtBounds
            DS.ScrollPhysics {target: choices}
            model: root.visible ? root.matches : []
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: option
                required property var modelData
                width: choices.width-Theme.scrollGutter
                text: modelData.path || modelData.title
                checkable: true
                checked: root.selectedIds.indexOf(modelData.id)>=0
                contentItem: RowLayout {
                    Label {text: option.text;Layout.fillWidth: true;elide: Text.ElideRight}
                    AppIcon {name:"check";visible:option.checked;ink:DS.DesignTokens.accent;Layout.preferredWidth:16;Layout.preferredHeight:16}
                }
                onClicked: {root.selected(modelData.kind || "tag",modelData.id,checked);if(!root.multiple)root.close()}
            }
            Label {anchors.centerIn: parent;visible: choices.count===0;text: search.text ? qsTr("没有匹配项") : qsTr("暂无条目，可在下方新建");wrapMode: Text.Wrap;width: parent.width;color: DS.DesignTokens.secondary}
        }
        Flow {
            Layout.fillWidth: true;spacing: 6
            Button {visible: root.multiple;text: qsTr("新建标签…");onClicked: {root.close();root.createRequested("tag")}}
            Button {visible: !root.multiple;text: qsTr("新建分类…");onClicked: {root.close();root.createRequested("project")}}
        }
    }
}
