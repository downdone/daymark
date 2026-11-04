#pragma once
#include <QList>
#include <QPointF>
#include <QRect>
#include <QString>
#include <array>

namespace daymark {
struct WidgetScreen { QString id; QRect available; };
struct WidgetAnchor {
    QString screen;
    QRect reference;
    QPointF fraction{0.05, 0.05};
    bool right = false, bottom = false;
};
struct WidgetCanvasLayout { QRect canvas; std::array<QRect,3> forms; };
struct WidgetPlacementPolicy {
    static int screenFor(const QRect& rectangle, const QList<WidgetScreen>& screens) {
        int best = -1; qint64 area = 0;
        for (int i = 0; i < screens.size(); ++i) {
            const auto overlap = rectangle.intersected(screens[i].available);
            const qint64 current = qint64(overlap.width()) * overlap.height();
            if (!overlap.isEmpty() && current > area) { area = current; best = i; }
        }
        return best;
    }
    static WidgetAnchor capture(const QRect& rect, const WidgetScreen& screen) {
        const auto bounds = screen.available;
        const bool right = rect.center().x() > bounds.center().x();
        const bool bottom = rect.center().y() > bounds.center().y();
        const auto point = QPoint(right ? rect.x() + rect.width() : rect.x(), bottom ? rect.y() + rect.height() : rect.y());
        return {screen.id, bounds, QPointF(double(point.x()-bounds.x())/qMax(1,bounds.width()),
                                         double(point.y()-bounds.y())/qMax(1,bounds.height())), right, bottom};
    }
    static QRect resolve(const WidgetAnchor& anchor, QSize size, const QList<WidgetScreen>& screens) {
        if (screens.isEmpty()) return {};
        const WidgetScreen* target = &screens.first(); bool found = false;
        for (const auto& screen : screens) if (screen.id == anchor.screen) { target = &screen; found = true; break; }
        const auto available = target->available;
        size = size.boundedTo(available.size());
        QPoint point = found ? available.topLeft() + QPoint(qRound(qBound(0.,anchor.fraction.x(),1.)*available.width()), qRound(qBound(0.,anchor.fraction.y(),1.)*available.height()))
                             : available.topLeft()+QPoint(24,24);
        if (found && anchor.right) point.rx() -= size.width();
        if (found && anchor.bottom) point.ry() -= size.height();
        point.setX(qBound(available.left(),point.x(),available.x()+available.width()-size.width()));
        point.setY(qBound(available.top(),point.y(),available.y()+available.height()-size.height()));
        return {point,size};
    }
    static WidgetCanvasLayout canvasLayout(const WidgetAnchor& anchor,const std::array<QSize,3>& sizes,int enabled,const QList<WidgetScreen>& screens) {
        WidgetCanvasLayout result;
        for(int form=0;form<3;++form){result.forms[form]=resolve(anchor,sizes[form],screens);if(enabled & (1<<form))result.canvas=result.canvas.united(result.forms[form]);}
        for(auto& rect:result.forms)rect.translate(-result.canvas.topLeft());
        return result;
    }

};
}
