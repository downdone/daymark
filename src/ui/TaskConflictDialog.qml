pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS

Dialog {
    id: root
    required property TaskDraftState draftState
    property var mine:({})
    property var latest:({})
    property var rows:[]
    property var choices:({})
    property var continuation:null
    readonly property bool unresolved:rows.some(row=>!choices[row.key])
    modal:true
    closePolicy:Popup.NoAutoClose
    title:qsTr("合并任务修改")
    width:Math.min(500*Theme.scale,parent ? parent.width-24 : 500)
    height:Math.min(implicitHeight,parent ? parent.height-24 : 640)
    x:parent ? Math.max(12,(parent.width-width)/2) : 0
    y:parent ? Math.max(12,(parent.height-height)/2) : 0
    function begin(base,local,remote,done){
        mine=local;latest=remote;continuation=done
        const labels={title:qsTr("标题"),notes:qsTr("备注"),arrangement:qsTr("日期、周期与提醒"),
            organization:qsTr("分类与父任务"),tags:qsTr("标签"),important:qsTr("特别关注"),pinned:qsTr("置顶")}
        const options={},fields=[]
        for(const key of Object.keys(local)){
            if(draftState.same(local[key],remote[key]))options[key]="latest"
            else if(draftState.same(local[key],base[key]))options[key]="latest"
            else if(draftState.same(remote[key],base[key]))options[key]="mine"
            else fields.push({key:key,label:labels[key]})
        }
        choices=options;rows=fields
        open()
    }
    function choose(key,value){choices=Object.assign({},choices,{[key]:value})}
    contentItem:DS.ScrollView {
        implicitHeight:Math.min(500*Theme.scale,body.implicitHeight)
        contentWidth:availableWidth
        ColumnLayout {
            id:body;width:parent.width;spacing:8
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:root.rows.length ? qsTr("以下内容在两处都被修改，请逐项选择。其他修改会自动合并。") : qsTr("两处修改可自动合并。应用后可继续检查并保存。")}
            Repeater {
                model:root.rows
                delegate:ColumnLayout {
                    id:fieldRow
                    required property var modelData
                    Layout.fillWidth:true
                    Label {text:fieldRow.modelData.label;font.weight:Font.DemiBold}
                    RadioButton {
                        objectName:root.objectName+"-mine-"+fieldRow.modelData.key
                        text:qsTr("我的修改");checked:root.choices[fieldRow.modelData.key]==="mine"
                        onClicked:root.choose(fieldRow.modelData.key,"mine")
                    }
                    TextArea {Layout.fillWidth:true;implicitHeight:Math.max(Theme.controlHeight,contentHeight+topPadding+bottomPadding);readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;text:root.draftState.describe(fieldRow.modelData.key,root.mine[fieldRow.modelData.key])}
                    RadioButton {
                        objectName:root.objectName+"-latest-"+fieldRow.modelData.key
                        text:qsTr("最新内容");checked:root.choices[fieldRow.modelData.key]==="latest"
                        onClicked:root.choose(fieldRow.modelData.key,"latest")
                    }
                    TextArea {Layout.fillWidth:true;implicitHeight:Math.max(Theme.controlHeight,contentHeight+topPadding+bottomPadding);readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;text:root.draftState.describe(fieldRow.modelData.key,root.latest[fieldRow.modelData.key])}
                }
            }
        }
    }
    footer:ActionBar {
        Button {id:continueEditing;text:qsTr("继续编辑");onClicked:root.reject()}
        Button {
            objectName:root.objectName+"-apply"
            text:qsTr("应用合并");enabled:!root.unresolved;highlighted:true
            onClicked:{
                const merged={}
                for(const key of Object.keys(root.mine))
                    merged[key]=root.choices[key]==="mine" ? root.mine[key] : root.latest[key]
                const done=root.continuation
                root.continuation=null
                root.close()
                if(done)done(merged)
            }
        }
    }
    onRejected:continuation=null
    onOpened:continueEditing.forceActiveFocus()
    Shortcut {sequence:"Escape";enabled:root.visible;onActivated:root.reject()}
}
