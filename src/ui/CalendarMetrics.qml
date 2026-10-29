import QtQuick

QtObject {
    property bool smallSurface: false
    readonly property int bodySize: smallSurface ? Math.round(12*Theme.scale) : Theme.bodySize
    readonly property int captionSize: smallSurface ? Math.round(11*Theme.scale) : Theme.captionSize
    readonly property int footnoteSize: smallSurface ? Math.round(11*Theme.scale) : Theme.footnoteSize
    readonly property int headingSize: smallSurface ? Math.round(15*Theme.scale) : Theme.headingSize
    readonly property int sectionSize: smallSurface ? Math.round(14*Theme.scale) : Theme.sectionSize
    readonly property int padding: smallSurface ? 8 : Theme.panelPadding
    readonly property int gap: smallSurface ? 4 : Theme.gap
    readonly property int weekMinimum: smallSurface ? 56 : 64
    readonly property int dateHeading: smallSurface ? 24 : 28
}
