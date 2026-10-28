#include "app/AppRuntime.h"

#include <QApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <cstdio>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QFontDatabase>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    auto interfaceFont=QFontDatabase::systemFont(QFontDatabase::GeneralFont);
#ifdef Q_OS_WIN
    // Use one installed CJK-capable UI face for Han, Latin and digits, with
    // the same policy in native surfaces and isolated probes.
    const auto families=QFontDatabase::families();
    if(families.contains(QStringLiteral("Microsoft YaHei UI")))
        interfaceFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
    else if(families.contains(QStringLiteral("Segoe UI")))
        interfaceFont.setFamily(QStringLiteral("Segoe UI"));
#endif
    interfaceFont.setPixelSize(13);
    interfaceFont.setWeight(QFont::Normal);
    interfaceFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(interfaceFont);
    QCoreApplication::setApplicationName(QStringLiteral("Daymark"));
    QCoreApplication::setOrganizationName(QStringLiteral("Daymark"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DAYMARK_VERSION));
    QIcon applicationIcon;
    for(const int size:{16,32,48,256})applicationIcon.addFile(QStringLiteral(":/daymark/icons/daymark-%1.png").arg(size));
    QApplication::setWindowIcon(applicationIcon);
    QQuickStyle::setStyle(QStringLiteral("DaymarkStyle"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Daymark %1 — offline tasks and calendar").arg(QCoreApplication::applicationVersion()));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Check QML startup and exit without user data access.")});
    parser.addOption({QStringLiteral("contextual-workspace-smoke"),QStringLiteral("Check the shared task inspector with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("continuous-calendar-smoke"),QStringLiteral("Check continuous calendar with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("calendar-interaction-smoke"),QStringLiteral("Check desktop calendar pointer and keyboard interactions with an isolated fixture.")});
    parser.addOption({QStringLiteral("desktop-surfaces-smoke"),QStringLiteral("Check floating, mini and desktop calendar with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("task-list-smoke"),QStringLiteral("Check desktop selection and task menus with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("Use an explicit local data directory."), QStringLiteral("directory")});
    parser.addOption({QStringLiteral("screenshot"),QStringLiteral("Save the synthetic smoke-test window image."),QStringLiteral("path")});
    parser.addOption({QStringLiteral("soak-seconds"),QStringLiteral("After isolated smoke, run a continuous synthetic stability test (10..43200 seconds)."),QStringLiteral("seconds")});
    parser.addOption({QStringLiteral("soak-report"),QStringLiteral("Write a new JSON stability report; requires --soak-seconds."),QStringLiteral("path")});
    parser.process(app);
    int soakSeconds=0;
    if(parser.isSet(QStringLiteral("soak-seconds"))) {
        bool valid=false;soakSeconds=parser.value(QStringLiteral("soak-seconds")).toInt(&valid);
        const auto report=QFileInfo(parser.value(QStringLiteral("soak-report")));
        if(!valid || soakSeconds<10 || soakSeconds>43200 || parser.isSet(QStringLiteral("data-dir"))
           || parser.value(QStringLiteral("soak-report")).isEmpty() || report.exists() || !report.dir().exists()) {
            std::fprintf(stderr,"Invalid stability options: use 10..43200 seconds, a new report path in an existing directory, and no --data-dir.\n");return 2;
        }
    } else if(parser.isSet(QStringLiteral("soak-report"))) {std::fprintf(stderr,"--soak-report requires --soak-seconds.\n");return 2;}
    const bool verification=parser.isSet(QStringLiteral("calendar-interaction-smoke")) || parser.isSet(QStringLiteral("desktop-surfaces-smoke")) || parser.isSet(QStringLiteral("task-list-smoke")) || parser.isSet(QStringLiteral("smoke-test")) || parser.isSet(QStringLiteral("continuous-calendar-smoke")) || parser.isSet(QStringLiteral("contextual-workspace-smoke")) || soakSeconds>0;
    if(verification) {
        qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){
            const auto utf8=message.toUtf8();
            std::fprintf(stderr,"%s\n",utf8.constData());std::fflush(stderr);
        });
    }

    daymark::AppRuntime runtime;
    if (!runtime.start(verification,parser.value(QStringLiteral("data-dir")),parser.value(QStringLiteral("screenshot")),soakSeconds,parser.value(QStringLiteral("soak-report")),parser.isSet(QStringLiteral("calendar-interaction-smoke"))?QStringLiteral("calendar"):parser.isSet(QStringLiteral("desktop-surfaces-smoke"))?QStringLiteral("desktop"):parser.isSet(QStringLiteral("task-list-smoke"))?QStringLiteral("selection"):parser.isSet(QStringLiteral("continuous-calendar-smoke"))?QStringLiteral("continuous"):parser.isSet(QStringLiteral("contextual-workspace-smoke"))?QStringLiteral("contextual"):QString()))
        return runtime.startupExitCode();
    const auto code=app.exec();
    if(verification && !runtime.verificationCompleted()){qCritical()<<"Verification exited before all scenarios completed, phase"<<runtime.verificationPhase();return code==0?3:code;}
    return code;
}
