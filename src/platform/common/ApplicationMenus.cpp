#include "ApplicationMenus.h"
#include <QAction>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

namespace daymark {
namespace {
#ifdef Q_OS_MACOS
// Dispatch to the editor, not the window's shortcut map (which owns this action).
void deliverEditingShortcut(const QKeySequence& shortcut)
{
    auto* window=qobject_cast<QQuickWindow*>(QGuiApplication::focusWindow());
    QPointer<QQuickItem> target=window?window->activeFocusItem():nullptr;
    if(!target || shortcut.isEmpty())return;
    const auto combination=shortcut[0];
    QKeyEvent press(QEvent::KeyPress,combination.key(),combination.keyboardModifiers());
    QKeyEvent release(QEvent::KeyRelease,combination.key(),combination.keyboardModifiers());
    QCoreApplication::sendEvent(target,&press);
    if(target)QCoreApplication::sendEvent(target,&release);
}
#endif
}
ApplicationMenus::ApplicationMenus(std::function<void(const QString&)> command)
{
#ifdef Q_OS_MACOS
    if(QGuiApplication::platformName()!=QStringLiteral("cocoa"))return;
    m_bar=std::make_unique<QMenuBar>();
    m_bar->setNativeMenuBar(true);
    auto add=[&](QMenu* menu,const QString& text,const QString& action,const QKeySequence& key={},QAction::MenuRole role=QAction::NoRole){
        auto* item=menu->addAction(text);
        item->setObjectName(QStringLiteral("native-action-")+action);
        item->setShortcut(key);
        item->setMenuRole(role);
        QObject::connect(item,&QAction::triggered,m_bar.get(),[command,action]{command(action);});
    };
    auto* application=m_bar->addMenu(QObject::tr("Daymark"));
    add(application,QObject::tr("关于Daymark"),QStringLiteral("about"),{},QAction::AboutRole);
    add(application,QObject::tr("设置…"),QStringLiteral("settings"),QKeySequence::Preferences,QAction::PreferencesRole);
    add(application,QObject::tr("退出Daymark"),QStringLiteral("quit"),QKeySequence::Quit,QAction::QuitRole);
    auto* file=m_bar->addMenu(QObject::tr("文件"));
    add(file,QObject::tr("新建任务"),QStringLiteral("new"),QKeySequence::New);
    add(file,QObject::tr("保存任务"),QStringLiteral("save"),QKeySequence::Save);
    file->addSeparator();
    add(file,QObject::tr("数据与备份…"),QStringLiteral("data"));

    struct EditingAction {const char* id;QString title;QKeySequence::StandardKey key;};
    const QList<EditingAction> editing={
        {"undo",QObject::tr("撤销"),QKeySequence::Undo},
        {"redo",QObject::tr("重做"),QKeySequence::Redo},
        {"cut",QObject::tr("剪切"),QKeySequence::Cut},
        {"copy",QObject::tr("复制"),QKeySequence::Copy},
        {"paste",QObject::tr("粘贴"),QKeySequence::Paste},
        {"selectAll",QObject::tr("全选"),QKeySequence::SelectAll}
    };
    auto* edit=m_bar->addMenu(QObject::tr("编辑"));
    for(const auto& definition:editing){
        auto* item=edit->addAction(definition.title);
        item->setObjectName(QStringLiteral("native-edit-")+QString::fromLatin1(definition.id));
        const QKeySequence shortcut(definition.key);
        item->setShortcut(shortcut);
        QObject::connect(item,&QAction::triggered,m_bar.get(),[shortcut]{deliverEditingShortcut(shortcut);});
    }
    auto* view=m_bar->addMenu(QObject::tr("视图"));
    add(view,QObject::tr("列表"),QStringLiteral("list"));
    for(const auto& pair:QList<QPair<QString,QString>>{{QObject::tr("日"),"day"},{QObject::tr("周"),"week"},{QObject::tr("月"),"month"},{QObject::tr("年"),"year"}})
        add(view,pair.first,pair.second);
    view->addSeparator();
    add(view,QObject::tr("今天"),QStringLiteral("calendarToday"));
    add(view,QObject::tr("前往日期…"),QStringLiteral("calendarGoTo"));
    add(view,QObject::tr("上一个时段"),QStringLiteral("calendarPrevious"));
    add(view,QObject::tr("下一个时段"),QStringLiteral("calendarNext"));
    auto* window=m_bar->addMenu(QObject::tr("窗口"));
    add(window,QObject::tr("主窗口"),QStringLiteral("main"));
    add(window,QObject::tr("显示/隐藏侧栏"),QStringLiteral("sidebar"));
    add(window,QObject::tr("桌面日历"),QStringLiteral("desktopCalendar"));
    auto* help=m_bar->addMenu(QObject::tr("帮助"));
    add(help,QObject::tr("键盘与使用提示"),QStringLiteral("help"));
#else
    Q_UNUSED(command);
#endif
}
ApplicationMenus::~ApplicationMenus()=default;
}
