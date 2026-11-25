pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS

Label {
    id: label
    property string fullPath: text
    property bool hintEnabled: true
    property bool hintBlocked: false

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    textFormat: Text.PlainText
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter

    function hintForVerification() {
        return {visible: nameTip.visible, text: nameTip.text,
                hovered: nameHover.hovered, blocked: hintBlocked || Theme.hintsBlocked || nameTip.blocked,
                attached: nameTip.parent === label, fitsOutsideSource: nameTip.fitsOutsideSource}
    }

    HoverHandler {
        id: nameHover
        enabled: label.hintEnabled && label.enabled
    }
    DS.ToolTip {
        id: nameTip
        visible: label.hintEnabled && label.enabled && label.visible && label.truncated
                 && nameHover.hovered && !label.hintBlocked && !Theme.hintsBlocked
                 && !blocked && text.length > 0
        text: label.fullPath || label.text
    }
}
