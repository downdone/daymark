#include "NativeWindowLayer.h"
namespace daymark {
WindowResult applyNativeWindowLayer(QWindow*,WindowLayer layer)
{return layer==WindowLayer::Desktop?WindowResult{WindowError::OperationRejected}:WindowResult{};}
bool nativeWindowLayerMatches(QWindow*,WindowLayer){return false;}
bool nativeWindowAbove(QWindow*,QWindow*){return false;}
bool nativeWindowRegionMatches(QWindow*,const QRect&){return false;} // Not verified by these backends.

}
