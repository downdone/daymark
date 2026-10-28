pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: row
    required property TaskWorkspace workspace
    required property string taskId
    required property string title
    required property real rowVersion
    required property string arrangementSummary
    required property string parentTitle
    required property int subtaskTotal
    required property bool isOverdue
    required property bool important
    property bool reduceMotion: false
    property bool completing: false
    property bool removing: false
    signal openRequested(string id)
    objectName: "widgetTask-"+taskId
    readonly property string summary: [parentTitle, arrangementSummary].filter(value => value.length>0).join(" · ")
    function complete() {
        if(!workspace.ready || workspace.busy || completing)return
        completing=workspace.changeState(taskId,rowVersion,"complete")
    }
    ListView.onRemove: {
        if(completing && !reduceMotion) {
            removing=true
            ListView.delayRemove=true
            removal.start()
        }
    }
    SequentialAnimation {
        id: removal
        PauseAnimation {duration: 110}
        ParallelAnimation {
            NumberAnimation {target: row;property: "opacity";to: 0;duration: 80;easing.type: Easing.OutQuad}
            NumberAnimation {target: row;property: "height";to: 0;duration: 80;easing.type: Easing.OutQuad}
        }
        ScriptAction {script: row.ListView.delayRemove=false}
    }
    Connections {
        target: row.workspace
        function onOperationFinished(operation,success) {
            if(operation==="complete" && !success)row.completing=false
        }
    }
    implicitHeight: Math.max(Theme.taskRowHeight,body.implicitHeight+8)
    leftPadding: 0;rightPadding: 6;topPadding: 4;bottomPadding: 4
    Accessible.name: title+(summary.length ? ", "+summary : "")
    onClicked: openRequested(taskId)
    Keys.onSpacePressed: complete()
    contentItem: RowLayout {
        id: body
        spacing: 4
        IconToolButton {
            objectName: "widgetComplete-"+row.taskId
            hintEnabled: false
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 32
            Layout.preferredHeight: Math.max(32,Theme.iconSize+8)
            enabled: row.workspace.ready && !row.workspace.busy && !row.completing
            Accessible.name: qsTr("完成任务：%1").arg(row.title)
            onClicked: row.complete()
            contentItem: Item {
                implicitWidth: Theme.iconSize;implicitHeight: Theme.iconSize
                Rectangle {
                    anchors.centerIn: parent
                    width: Theme.iconSize;height: width;radius: width/2
                    color: row.removing ? Theme.accent : "transparent"
                    border.color: row.removing ? Theme.accent : Theme.secondary
                    AppIcon {anchors.centerIn: parent;width: Theme.iconSize*2/3;height: width;name: "check";ink: Theme.accentText;visible: row.removing}
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true;Layout.minimumWidth: 0
            spacing: 2
            HoverHandler {id: textHover}
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                AppIcon {
                    visible: row.parentTitle.length>0 || row.subtaskTotal>0
                    name: row.parentTitle.length>0 ? "subtask" : "subtasks"
                    Layout.preferredWidth: Theme.iconSize;Layout.preferredHeight: Theme.iconSize
                    Accessible.name: row.parentTitle.length>0 ? qsTr("子任务") : qsTr("包含子任务")
                }
                Label {
                    id: titleLabel
                    Layout.fillWidth: true;Layout.minimumWidth: 0
                    text: row.title;textFormat: Text.PlainText;font.pixelSize: Theme.listTitleSize
                    font.strikeout: row.removing;color: row.removing ? Theme.secondary : Theme.text
                    elide: Text.ElideRight
                }
                AppIcon {visible: row.important;name: "flag";ink: Theme.accent;Layout.preferredWidth: Theme.iconSize;Layout.preferredHeight: Theme.iconSize;Accessible.name: qsTr("特别关注")}
            }
            Label {
                id: summaryLabel
                visible: row.summary.length>0
                Layout.fillWidth: true;Layout.minimumWidth: 0
                text: row.summary;textFormat: Text.PlainText;font.pixelSize: Theme.captionSize
                color: row.isOverdue ? Theme.danger : Theme.secondary;elide: Text.ElideRight
            }
        }
    }
    ToolTip.visible: !Theme.hintsBlocked && textHover.hovered && !down && !completing && (titleLabel.truncated || summaryLabel.truncated)
    ToolTip.delay: Theme.hintDelay
    ToolTip.timeout: Theme.hintTimeout
    ToolTip.text: title+(summary.length ? "\n"+summary : "")
    background: Rectangle {
        radius: Theme.rowRadius
        color: row.down ? Theme.pressed : row.hovered ? Theme.hover : "transparent"
        border.width: row.visualFocus || row.highlighted ? 1 : 0;border.color: Theme.accent
    }
}
