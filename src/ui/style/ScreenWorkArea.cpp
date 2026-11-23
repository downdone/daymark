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
