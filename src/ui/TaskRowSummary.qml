pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: summary
    required property string taskId
    required property string title
    required property string notes
    required property string metadataText
    required property bool done
    required property bool overdue
    required property bool showSubtasks
    required property int subtaskTotal
    required property int subtaskPending
    property bool interactive: true
    readonly property Item titleItem: titleLabel
    readonly property Item notesItem: notesLabel
    signal subtasksRequested()
    spacing: 1
    function verifyGeometry() {
        for(const child of children)
            if(child.visible && child.y+child.height>height+1)return "summary child exceeds row"
        for(const child of metadata.children)
            if(child.visible && child.y+child.height>metadata.height+1)return "metadata exceeds row"
        return "PASS"
    }
    Label {
        id: titleLabel
        objectName: "taskTitle-"+summary.taskId
        Layout.fillWidth: true
        topPadding: Math.max(0,(Math.max(32,Theme.iconSize+12)-Theme.listTitleSize*1.35)/2)
        text: summary.title
        textFormat: Text.PlainText
        font.pixelSize: Theme.listTitleSize
        font.strikeout: summary.done
        color: summary.done ? Theme.secondary : Theme.text
        elide: Text.ElideRight
    }
    Label {
        id: notesLabel
        objectName: "taskNotes-"+summary.taskId
        Layout.fillWidth: true
        visible: summary.notes.trim().length>0
        text: summary.notes.trim().replace(/\s+/g," ")
        textFormat: Text.PlainText
        font.pixelSize: Theme.captionSize
        color: Theme.secondary
        elide: Text.ElideRight
    }
    RowLayout {
        id: metadata
        Layout.fillWidth: true
        visible: summary.showSubtasks || summary.metadataText.length>0
        spacing: 6
        ToolButton {
            id: subtaskLink
            objectName: "taskSubtasksLink-"+summary.taskId
            visible: summary.showSubtasks
            enabled: summary.interactive
            text: qsTr("%1 个子任务").arg(summary.subtaskTotal)
            font.pixelSize: Theme.captionSize
            padding: 0
            implicitHeight: Math.max(32,Theme.captionSize+12)
            contentItem: Text {text: subtaskLink.text;font: subtaskLink.font;color: !subtaskLink.enabled ? Theme.disabledText : subtaskLink.hovered || subtaskLink.activeFocus ? Theme.accent : Theme.secondary;verticalAlignment: Text.AlignVCenter}
            Accessible.name: qsTr("%1：%2 个子任务，已完成 %3 个。查看子任务").arg(summary.title).arg(summary.subtaskTotal).arg(summary.subtaskTotal-summary.subtaskPending)
            background: Rectangle {radius: 4;color: "transparent";border.width: subtaskLink.visualFocus ? 1 : 0;border.color: Theme.accent}
            onClicked: summary.subtasksRequested()
        }
        Label {
            objectName: "taskMetadata-"+summary.taskId
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: summary.metadataText
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: Theme.captionSize
            color: summary.overdue ? Theme.danger : Theme.secondary
            ToolTip.visible: !Theme.hintsBlocked && metadataHover.hovered && truncated
            ToolTip.text: summary.metadataText
            ToolTip.delay: Theme.hintDelay
            ToolTip.timeout: Theme.hintTimeout
            HoverHandler {id: metadataHover}
        }
    }
}
