#pragma once

#include <QObject>
#include <QQuickItem>
#include <QRect>
#include <QtQml/qqmlregistration.h>

class ScreenWorkArea : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  Q_INVOKABLE QRect forItem(QQuickItem *item) const;
};
