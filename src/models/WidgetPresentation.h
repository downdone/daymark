#pragma once
#include <QList>

namespace daymark {
// Stable user intent, independent of native windows and animation frames.
struct WidgetPresentation {
    enum Form { Panel = 0, Mini = 1, Ball = 2 };
    int enabled = (1 << Panel) | (1 << Mini);
    Form selected = Panel;
    Form effective = Panel;
    bool temporaryOpen = false;
    QList<Form> history{Panel};
    bool allows(int form) const { return form >= Panel && form <= Ball && (enabled & (1 << form)); }
    int count() const { return int(allows(Panel)) + int(allows(Mini)) + int(allows(Ball)); }
    Form fallback(bool compactOnly = false) const {
        for (auto form : history) if (allows(form) && (!compactOnly || form != Panel)) return form;
        for (auto form : {Panel, Mini, Ball}) if (allows(form) && (!compactOnly || form != Panel)) return form;
        return selected; // Empty sets stay hidden; this is retained intent, not a render target.
    }
    bool select(int form) {
        if (!allows(form)) return false;
        selected = effective = Form(form);
        temporaryOpen = false;
        history.removeAll(selected); history.prepend(selected);
        return true;
    }
    void setEnabled(int mask) {
        enabled = mask & 7;
        if (!allows(selected) && enabled) { selected = effective = fallback(); temporaryOpen = false; }
        if (!allows(effective)) { effective = selected; temporaryOpen = false; }
        if (!enabled || effective == selected) temporaryOpen = false;
    }
    bool preview(int form) {
        if (!allows(form) || selected == Panel || form == Panel || temporaryOpen) return false;
        effective = Form(form); return true;
    }
    int primaryTarget() const {
        if (allows(Panel)) return Panel;
        if (effective == Ball && allows(Mini)) return Mini;
        return -1; // Open the main workspace.
    }
    bool openPrimary() {
        const int target = primaryTarget();
        if (target < 0) return false;
        effective = Form(target);
        temporaryOpen = effective != selected;
        return true;
    }
    bool returnToSelected() {
        if (!allows(selected)) return false;
        effective = selected; temporaryOpen = false; return true;
    }
};
}
