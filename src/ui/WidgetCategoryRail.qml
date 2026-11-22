pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id:root
    required property TaskWorkspace workspace
    property bool blocked:false
    property real cornerRadius:Theme.panelRadius
    signal contextRequested(string key)
    signal categoriesRequested()
    signal dropBrowseRequested()
    signal taskMoved(string destination)
    signal dropError(string message)
    padding:6
    implicitWidth:104*Theme.scale
    background:Rectangle {color:Theme.sidebar;topLeftRadius:root.cornerRadius;bottomLeftRadius:root.cornerRadius;Rectangle {anchors.right:parent.right;width:1;height:parent.height;color:Theme.detailDivider}}
    function focusCategories(){allCategories.forceActiveFocus()}
    function focusStep(step){
        const buttons=[today]
        for(let i=0;i<shortcuts.count;i++)buttons.push(shortcuts.itemAt(i))
        buttons.push(allCategories)
        const next=(Math.max(0,buttons.indexOf(Window.window.activeFocusItem))+step+buttons.length)%buttons.length
        buttons[next].forceActiveFocus(Qt.TabFocusReason)
    }
    component Entry:ToolButton {
        id:entry
        property string symbol:"folder"
        property string fullPath:text
        property string contextKey:""
        property bool dropBrowse:false
        Accessible.selected:checked
        Layout.fillWidth:true;Layout.minimumWidth:0
        padding:6
        implicitHeight:Math.max(32,contentItem.implicitHeight+12)
        background:Rectangle {radius:Theme.controlRadius;color:entry.down?Theme.pressed:entry.checked?Theme.navigationSelection:entry.hovered?Theme.hover:"transparent";border.width:entry.visualFocus?1:0;border.color:Theme.accent}
        contentItem:RowLayout {
            spacing:5
            AppIcon {name:entry.symbol;Layout.preferredWidth:14*Theme.scale;Layout.preferredHeight:14*Theme.scale;ink:entry.checked?Theme.accent:Theme.secondary}
            Label {
                id:entryText;Layout.fillWidth:true;Layout.minimumWidth:0;text:entry.text
                font.pixelSize:Theme.captionSize;font.weight:entry.checked?Font.DemiBold:Font.Normal;color:Theme.text;elide:Text.ElideRight
                DS.ToolTip {visible:!Theme.hintsBlocked && entryText.truncated && labelHover.hovered;text:entry.fullPath}
                HoverHandler {id:labelHover}
            }
        }
        Keys.onUpPressed:root.focusStep(-1)
        Keys.onDownPressed:root.focusStep(1)
        Keys.onReturnPressed:clicked()
        WidgetCategoryDropTarget {
            visible:entry.contextKey.length>0 || entry.dropBrowse
            workspace:root.workspace;contextKey:entry.contextKey;destinationName:entry.fullPath
            blocked:root.blocked;browseOnly:entry.dropBrowse
            onBrowseRequested:root.dropBrowseRequested()
            onMoveAccepted:function(destination){root.taskMoved(destination)}
            onErrorReported:function(message){root.dropError(message)}
        }
    }
    contentItem:ColumnLayout {
        spacing:4
        Entry {id:today;objectName:"widgetBrowseToday";text:qsTr("今天");symbol:"today";checked:root.workspace.widgetContext==="today";enabled:!root.blocked;onClicked:root.contextRequested("today")}
        Label {text:qsTr("常用分类");font.pixelSize:Theme.captionSize;color:Theme.secondary;Layout.leftMargin:6;Layout.topMargin:6}
        ScrollView {
            id:favoriteScroll
            Layout.fillWidth:true;Layout.fillHeight:true
            contentWidth:availableWidth;clip:true;rightPadding:10
            ScrollBar.vertical:ScrollBar {parent:favoriteScroll;x:favoriteScroll.width-width;y:favoriteScroll.topPadding;width:10;height:favoriteScroll.availableHeight;policy:ScrollBar.AsNeeded}
            ColumnLayout {
                width:parent.width;spacing:2
                Repeater {
                    id:shortcuts;model:root.workspace.widgetShortcuts
                    delegate:Entry {
                        required property var modelData
                        objectName:"widgetShortcut-"+modelData.key
                        text:modelData.title;checked:root.workspace.widgetContext===modelData.key
                        fullPath:modelData.path
                        contextKey:modelData.key
                        Accessible.name:modelData.path
                        enabled:!root.blocked
                        onClicked:root.contextRequested(modelData.key)
                    }
                }
                Item {Layout.fillWidth:true;implicitHeight:1}
            }
        }
        Entry {id:allCategories;objectName:"widgetRailCategories";text:qsTr("分类");dropBrowse:true;enabled:!root.blocked;onClicked:root.categoriesRequested()}
    }
}
