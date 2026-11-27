#include "ScreenWorkArea.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

QRect ScreenWorkArea::forItem(QQuickItem *item) const {
  if (!item || !item->window())
    return {};

  const auto center = item->mapToGlobal(item->boundingRect().center()).toPoint();
  const auto *hostScreen = item->window()->screen();
  const auto *anchorScreen = hostScreen && hostScreen->geometry().contains(center)
                                 ? hostScreen
                                 : QGuiApplication::screenAt(center);
  const auto *screen = anchorScreen ? anchorScreen : hostScreen;
  return screen ? screen->availableGeometry() : QRect{};
}

QPointF ScreenWorkArea::beside(QQuickItem *anchor, QQuickItem *parent, qreal width, qreal height) const {
  if (!anchor || !parent) return {};
  const auto area = QRectF(forItem(anchor)).adjusted(6, 6, -6, -6);
  const auto top = anchor->mapToGlobal(QPointF());
  const auto right = anchor->mapToGlobal(QPointF(anchor->width(), 0)).x() + 8;
  const auto x = right + width <= area.right() ? right : top.x() - width - 8;
  return parent->mapFromGlobal(QPointF(qBound(area.left(), x, qMax(area.left(), area.right() - width)),
                                      qBound(area.top(), top.y(), qMax(area.top(), area.bottom() - height))));
}
