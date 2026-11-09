#include "app/AppRuntime.h"
#include "update/qt/StartupUpdateGate.h"
#include "platform/common/BrandIcons.h"
#include "diagnostics/RuntimeLog.h"

#include <QApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <cstdio>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QIcon>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QSysInfo>

namespace {
struct LogSession {
    std::unique_ptr<QTemporaryDir> temporary;
    LogSession(int argc,char** argv) {
        QStringList arguments;for(int i=1;i<argc;++i)arguments.append(QString::fromLocal8Bit(argv[i]));
        if(arguments.contains("--version") || arguments.contains("--help") || arguments.contains("-h"))return;
        bool isolated=false;QString data,explicitLogs;
        for(int i=0;i<arguments.size();++i){const auto& argument=arguments[i];
            if(argument=="--object-audit-report" || argument.startsWith("--object-audit-report=") || argument.endsWith("-smoke") || argument=="--smoke-test" || argument=="--update-preflight" || argument=="--soak-seconds")isolated=true;
            if(argument=="--data-dir" && i+1<arguments.size())data=arguments[i+1];
            if(argument=="--log-dir" && i+1<arguments.size())explicitLogs=arguments[i+1];
            if(argument.startsWith("--data-dir="))data=argument.mid(11);
            if(argument.startsWith("--log-dir="))explicitLogs=argument.mid(10);
            if(argument.startsWith("--soak-seconds="))isolated=true;
        }
        QString logs;
        if(isolated)qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){
            const auto bytes=message.toUtf8();std::fprintf(stderr,"%s\n",bytes.constData());std::fflush(stderr);
        }); // Preserve the probes' existing stderr sink beneath the global file handler.
        if(isolated){temporary=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/daymark-log-XXXXXX");if(temporary->isValid())logs=temporary->filePath("logs");}
        else if(!explicitLogs.isEmpty())logs=explicitLogs;
        else logs=QDir(data.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation):data).filePath("logs");
        if(!daymark::diagnostics::start(logs))std::fprintf(stderr,"Daymark local logging unavailable; application startup continues.\n");
        daymark::diagnostics::event("startup","process.started",{{"phase","before_qapplication"}});
    }
    ~LogSession(){daymark::diagnostics::event("startup","process.stopped");daymark::diagnostics::stop();}
};
}

int main(int argc, char* argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("Daymark"));
    QCoreApplication::setOrganizationName(QStringLiteral("Daymark"));
    LogSession logs(argc,argv);
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
#ifdef DAYMARK_UPDATE_FIXTURE_APP_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(DAYMARK_UPDATE_FIXTURE_APP_VERSION));
#else
    QCoreApplication::setApplicationVersion(QStringLiteral(DAYMARK_VERSION));
#endif
    QCryptographicHash binaryHash(QCryptographicHash::Sha256);QFile binary(QCoreApplication::applicationFilePath());
    if(binary.open(QIODevice::ReadOnly)){binaryHash.addData(&binary);binary.close();}
    daymark::diagnostics::event("startup","application.ready",{{"app_version",QCoreApplication::applicationVersion()},
        {"qt_version",QString::fromLatin1(qVersion())},{"platform",QGuiApplication::platformName()},
        {"os",QSysInfo::prettyProductName()},{"binary_sha256",QString::fromLatin1(binaryHash.result().toHex())}});
    QApplication::setWindowIcon(daymark::BrandIcons::application());
    QQuickStyle::setStyle(QStringLiteral("DaymarkStyle"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Daymark %1 — offline tasks and calendar").arg(QCoreApplication::applicationVersion()));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("update-preflight"),QStringLiteral("Validate a downloaded payload using only temporary synthetic data.")});
    parser.addOption({QStringLiteral("update-handoff"),QStringLiteral("Authenticated local update startup capability."),QStringLiteral("handle")});
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Check QML startup and exit without user data access.")});
    parser.addOption({QStringLiteral("contextual-workspace-smoke"),QStringLiteral("Check the shared task inspector with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("continuous-calendar-smoke"),QStringLiteral("Check continuous calendar with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("calendar-interaction-smoke"),QStringLiteral("Check desktop calendar pointer and keyboard interactions with an isolated fixture.")});
    parser.addOption({QStringLiteral("object-audit-report"),QStringLiteral("Audit object ownership using isolated synthetic data; new report path required."),QStringLiteral("path")});
    parser.addOption({QStringLiteral("calendar-lifecycle-smoke"),QStringLiteral("Verify disposable calendar contexts with isolated synthetic data.")});
    parser.addOption({QStringLiteral("title-bar-smoke"),QStringLiteral("Verify Windows title bar preferences and native window controls with isolated data.")});
    parser.addOption({QStringLiteral("title-layout-smoke"),QStringLiteral("Verify stable window layouts and update notification UI with isolated data.")});
    parser.addOption({QStringLiteral("picker-lifecycle-smoke"),QStringLiteral("Verify lazy date/time picker ownership with isolated data.")});
    parser.addOption({QStringLiteral("widget-pointer-smoke"),QStringLiteral("Verify widget button and pointer routing with isolated synthetic data.")});
    parser.addOption({QStringLiteral("time-input-smoke"),QStringLiteral("Verify smart time input using isolated synthetic data.")});
    parser.addOption({QStringLiteral("desktop-surfaces-smoke"),QStringLiteral("Check floating, mini and desktop calendar with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("task-list-smoke"),QStringLiteral("Check desktop selection and task menus with an isolated synthetic fixture.")});
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("Use an explicit local data directory."), QStringLiteral("directory")});
    parser.addOption({QStringLiteral("log-dir"),QStringLiteral("Write bounded local diagnostic logs to this directory."),QStringLiteral("directory")});
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
    if(parser.isSet(QStringLiteral("object-audit-report"))){
        const QFileInfo report(parser.value(QStringLiteral("object-audit-report")));
        if(report.fileName().isEmpty() || report.exists() || !report.dir().exists() || parser.isSet(QStringLiteral("data-dir")) || parser.isSet(QStringLiteral("update-handoff")) || parser.isSet(QStringLiteral("screenshot")) || soakSeconds>0)return 2;
    }
    const bool verification=parser.isSet(QStringLiteral("title-layout-smoke")) || parser.isSet(QStringLiteral("calendar-lifecycle-smoke")) || parser.isSet(QStringLiteral("title-bar-smoke")) || parser.isSet(QStringLiteral("picker-lifecycle-smoke")) || parser.isSet(QStringLiteral("object-audit-report")) || parser.isSet(QStringLiteral("widget-pointer-smoke")) || parser.isSet(QStringLiteral("time-input-smoke")) || parser.isSet(QStringLiteral("update-preflight")) || parser.isSet(QStringLiteral("calendar-interaction-smoke")) || parser.isSet(QStringLiteral("desktop-surfaces-smoke")) || parser.isSet(QStringLiteral("task-list-smoke")) || parser.isSet(QStringLiteral("smoke-test")) || parser.isSet(QStringLiteral("continuous-calendar-smoke")) || parser.isSet(QStringLiteral("contextual-workspace-smoke")) || soakSeconds>0;
    // The legacy full probe inspects QML menu geometry across timer phases.
    // Native menu loops are covered by the separate desktop menu fixtures.
    if(verification && (parser.isSet(QStringLiteral("smoke-test")) || qEnvironmentVariableIsSet("DAYMARK_SMOKE_MENU_FALLBACK")))
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuWindows);

    if(parser.isSet(QStringLiteral("update-handoff")) && verification)return 2;
    daymark::StartupUpdateGate updateGate;
    if(!verification && !updateGate.acquire(parser.value(QStringLiteral("update-handoff")),QCoreApplication::applicationVersion())){qCritical()<<updateGate.error();return 40;}
    daymark::AppRuntime runtime;
    if (!runtime.start(verification,parser.value(QStringLiteral("data-dir")),parser.isSet(QStringLiteral("object-audit-report"))?QFileInfo(parser.value(QStringLiteral("object-audit-report"))).absoluteFilePath():parser.value(QStringLiteral("screenshot")),soakSeconds,parser.value(QStringLiteral("soak-report")),parser.isSet(QStringLiteral("title-layout-smoke"))?QStringLiteral("title-layout"):parser.isSet(QStringLiteral("calendar-lifecycle-smoke"))?QStringLiteral("calendar-lifecycle"):parser.isSet(QStringLiteral("title-bar-smoke"))?QStringLiteral("title-bar"):parser.isSet(QStringLiteral("object-audit-report"))?QStringLiteral("object-audit"):parser.isSet(QStringLiteral("picker-lifecycle-smoke"))?QStringLiteral("picker-lifecycle"):parser.isSet(QStringLiteral("widget-pointer-smoke"))?QStringLiteral("widget-pointer"):parser.isSet(QStringLiteral("time-input-smoke"))?QStringLiteral("time-input"):parser.isSet(QStringLiteral("update-preflight"))?QStringLiteral("update-preflight"):parser.isSet(QStringLiteral("calendar-interaction-smoke"))?QStringLiteral("calendar"):parser.isSet(QStringLiteral("desktop-surfaces-smoke"))?QStringLiteral("desktop"):parser.isSet(QStringLiteral("task-list-smoke"))?QStringLiteral("selection"):parser.isSet(QStringLiteral("continuous-calendar-smoke"))?QStringLiteral("continuous"):parser.isSet(QStringLiteral("contextual-workspace-smoke"))?QStringLiteral("contextual"):QString(),&updateGate))
        return runtime.startupExitCode();
    const auto code=app.exec();
    daymark::diagnostics::event("startup","event_loop.exited",{{"exit_code",code}});
    if(verification && !runtime.verificationCompleted()){qCritical()<<"Verification exited before all scenarios completed, phase"<<runtime.verificationPhase();return code==0?3:code;}
    return code;
}
