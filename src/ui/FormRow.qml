pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
GridLayout {
    id: row
    objectName: "desktopFormRow"
    required property string label
    property string detail: ""
    property bool contextual: false
    property real labelWidth: contextual ? 56*Theme.scale : Theme.propertyLabelWidth
    property real minimumValueWidth: 180*Theme.scale
    readonly property bool stacked: width<labelWidth+minimumValueWidth+12
    default property alias controls: valueColumn.data
    Layout.fillWidth: true
    columns: stacked ? 1 : 2
    columnSpacing: contextual ? 8 : 12
    rowSpacing: 4
    function verifyLayout() {
        if(width<=0 || height<=0 || valueColumn.width<=0 || valueColumn.height<=0)return "empty form row"
        if(valueColumn.x<0 || valueColumn.x+valueColumn.width>width+1)return "form value outside row"
        if(stacked ? labelItem.y+labelItem.height>valueColumn.y+1 : labelItem.x+labelItem.width>valueColumn.x+1)return "form label overlaps value"
        return "PASS"
    }
    data: [Label {
        id: labelItem
        text: row.label
        Layout.preferredWidth: row.stacked ? -1 : row.labelWidth
        Layout.maximumWidth: row.stacked ? Infinity : row.labelWidth
        Layout.fillWidth: row.stacked
        Layout.alignment: Qt.AlignTop
        topPadding: row.stacked ? 0 : Math.max(0,(Theme.controlHeight-font.pixelSize*1.35)/2)
        horizontalAlignment: row.contextual && !row.stacked ? Text.AlignRight : Text.AlignLeft
        wrapMode: Text.Wrap
        font.pixelSize: Theme.navigationSize
        color: Theme.secondary
    }, ColumnLayout {
        id: valueColumn
        Layout.fillWidth: true
        spacing: 4
    }, Label {
        visible: row.detail.length>0
        text: row.detail
        Layout.columnSpan: row.stacked ? 1 : 2
        Layout.leftMargin: row.stacked ? 0 : row.labelWidth+12
        Layout.fillWidth: true
        font.pixelSize: Theme.captionSize
        wrapMode: Text.Wrap
        color: Theme.secondary
    }
]
}
