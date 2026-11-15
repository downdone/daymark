#include "TaskWorkspace.h"
#include "ReminderPresentation.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <algorithm>

namespace daymark {
QRect TaskWorkspace::reminderPopupArea() const
{
    const auto* screen=QGuiApplication::primaryScreen();
    return screen?screen->availableGeometry():QRect(0,0,800,600);
}
void TaskWorkspace::loadReminderPresentation()
{
    m_reminderCandidates.clear();m_reminderPresentation.clear();
    emit reminderInboxOpening();setReminderInboxActive(false);
    m_reminderUndoKey.clear();m_reminderUndoValue={};
    QSettings settings(m_path+QStringLiteral(".reminders.ini"),QSettings::IniFormat);
    const auto state=settings.value(QStringLiteral("presentation")).toMap();
    const auto since=m_clock.nowUtc().addDays(-30);
    for(auto it=state.cbegin();it!=state.cend();++it)
        if(QDateTime::fromString(it.value().toMap().value("updatedAt").toString(),Qt::ISODateWithMs)>=since)
            m_reminderPresentation.insert(it.key(),it.value());
    refreshReminderAlerts();
    emit reminderAlertsChanged();
}
void TaskWorkspace::refreshReminderAlerts()
{
    // Manual visibility is independent of the automatic popup preference.
    const auto pending=m_ready?ReminderPresentation::pendingUsing(m_reminderCandidates,
        [this](const QString& id){return cachedTask(id);},m_reminderPresentation,m_clock.nowUtc()):QVariantList{};
    if(pending!=m_reminderAlerts){m_reminderAlerts=pending;emit reminderAlertsChanged();}
}
bool TaskWorkspace::saveReminderPresentation(const QVariantMap& state)
{
    QSettings settings(m_path+QStringLiteral(".reminders.ini"),QSettings::IniFormat);
    settings.setValue(QStringLiteral("presentation"),state);settings.sync();
    if(settings.status()!=QSettings::NoError){settingsFailed();return false;}
    m_reminderPresentation=state;
    return true;
}
bool TaskWorkspace::acknowledgeReminder(const QString& key,int delayMinutes)
{
    if(!m_ready || (delayMinutes!=0 && delayMinutes!=10))return false;
    const auto found=std::find_if(m_reminderAlerts.cbegin(),m_reminderAlerts.cend(),
        [&](const QVariant& value){return value.toMap().value("key")==key;});
    if(found==m_reminderAlerts.cend())return false;
    const auto now=m_clock.nowUtc();auto next=m_reminderPresentation;
    next.insert(key,QVariantMap{{"dismissed",delayMinutes==0},
        {"until",delayMinutes?now.addSecs(delayMinutes*60).toString(Qt::ISODateWithMs):QString()},
        {"updatedAt",now.toString(Qt::ISODateWithMs)}});
    const auto old=m_reminderPresentation.value(key);
    if(!saveReminderPresentation(next))return false;
    m_reminderUndoKey=key;m_reminderUndoValue=old;
    refreshReminderAlerts();emit reminderAlertsChanged();return true;
}
bool TaskWorkspace::undoReminderAcknowledgement()
{
    if(!m_ready || m_reminderUndoKey.isEmpty())return false;
    auto next=m_reminderPresentation;
    if(m_reminderUndoValue.isValid())next.insert(m_reminderUndoKey,m_reminderUndoValue);
    else next.remove(m_reminderUndoKey);
    if(!saveReminderPresentation(next))return false;
    m_reminderUndoKey.clear();m_reminderUndoValue={};
    refreshReminderAlerts();emit reminderAlertsChanged();return true;
}
}
