#pragma once
#include "TaskTimeRecognition.h"
#include <QVariantList>

namespace daymark {
// Local, bounded preview only. Candidates are never persisted without confirmation.
class ActivityExtraction final {
public:
    static Result<QVariantList> preview(const QString& text,const QDateTime& now,const QTimeZone& zone);
};
}
