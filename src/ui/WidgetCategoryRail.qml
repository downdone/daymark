pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id:root
    required property TaskWorkspace workspace
    property bool blocked:false
    property real dividerOpacity:1
    signal contextRequested(string key)
    signal categoriesRequested()
    signal orderMenuRequested(var category, Item anchor, point position)
    signal reorderRequested(var category, int direction)
    signal dropBrowseRequested()
    signal taskMoved(string destination)
    signal dropError(string message)
    padding:6
    implicitWidth:104*Theme.scale
    background:Item {Rectangle {anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.topMargin:8;anchors.bottomMargin:8;width:1;color:Theme.detailDivider;opacity:root.dividerOpacity}}
    function focusCategories(){allCategories.forceActiveFocus()}
    function focusCategory(key) {
        const index=categories.model.findIndex(item=>item.key===key)
        if(index<0){focusCategories();return}
        categories.currentIndex=index
        categories.positionViewAtIndex(index,ListView.Contain)
        categories.forceActiveFocus(Qt.TabFocusReason)
        Qt.callLater(function(){if(categories.activeFocus && categories.currentItem)categories.currentItem.forceActiveFocus(Qt.TabFocusReason)})
    }
    function focusStep(step){
        const count=categories.count
        const current=today.activeFocus ? -1 : allCategories.activeFocus ? count : categories.currentIndex
        const next=(current+1+step+count+2)%(count+2)-1
        if(next<0)today.forceActiveFocus(Qt.TabFocusReason)
        else if(next===count)allCategories.forceActiveFocus(Qt.TabFocusReason)
        else focusCategory(categories.model[next].key)
    }
    component Entry:ToolButton {
        id:entry
        property string symbol:"folder"
        property string fullPath:text
        property string contextKey:""
        property bool dropBrowse:false
        property var category:null
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
        Keys.onUpPressed:function(event){if(entry.category && (event.modifiers & Qt.AltModifier))root.reorderRequested(entry.category,-1);else root.focusStep(-1)}
        Keys.onDownPressed:function(event){if(entry.category && (event.modifiers & Qt.AltModifier))root.reorderRequested(entry.category,1);else root.focusStep(1)}
        Keys.onReturnPressed:clicked()
        TapHandler {
            acceptedButtons:Qt.RightButton
            enabled:!!entry.category && entry.enabled
            onTapped:function(point){root.orderMenuRequested(entry.category,entry,point.position)}
        }
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
        Label {text:qsTr("分类");font.pixelSize:Theme.captionSize;color:Theme.secondary;Layout.leftMargin:6;Layout.topMargin:6}
        Item {
            Layout.fillWidth:true;Layout.fillHeight:true
            ListView {
                id:categories;objectName:"widgetCategoryRailList"
                anchors.fill:parent;anchors.rightMargin:Theme.scrollGutter;clip:true;spacing:2
                model:root.workspace.widgetCategories
                DS.ScrollPhysics {target:categories;keyboardNavigationEnabled:false}
                ScrollBar.vertical:ScrollBar {parent:categories.parent;x:categories.width;y:0;width:Theme.scrollGutter;height:categories.height;policy:ScrollBar.AsNeeded}
                delegate:Entry {
                    required property var modelData
                    required property int index
                    width:categories.width
                    leftPadding:6+modelData.depth*8*Theme.scale
                    objectName:"widgetShortcut-"+modelData.key
                    text:modelData.title;checked:root.workspace.widgetContext===modelData.key
                    fullPath:modelData.path;category:modelData
                    contextKey:modelData.key
                    Accessible.name:modelData.path
                    enabled:!root.blocked
                    onActiveFocusChanged:if(activeFocus)categories.currentIndex=index
                    onClicked:root.contextRequested(modelData.key)
                }
            }
        }
        Entry {id:allCategories;objectName:"widgetRailCategories";text:qsTr("查找分类");symbol:"search";dropBrowse:true;enabled:!root.blocked;onClicked:root.categoriesRequested()}
    }
}
