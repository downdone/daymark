#include "app/AppRuntime.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <cstdio>
#include <QFileInfo>
#include <QDir>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Daymark"));
    QCoreApplication::setOrganizationName(QStringLiteral("Daymark"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DAYMARK_VERSION));
    QIcon applicationIcon;
    for(const int size:{16,32,48,256})applicationIcon.addFile(QStringLiteral(":/daymark/icons/daymark-%1.png").arg(size));
    QApplication::setWindowIcon(applicationIcon);
    QQuickStyle::setStyle(QStringLiteral("DaymarkStyle"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Daymark Windows V1 release candidate"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Check QML startup and exit without user data access.")});
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
    const bool verification=parser.isSet(QStringLiteral("smoke-test")) || soakSeconds>0;
#ifdef Q_OS_WIN
    // The offscreen plugin has no Windows system-font integration. Use the
    // installed UI font for synthetic layout tests; native users keep the OS font.
    if(verification && QGuiApplication::platformName()==QStringLiteral("offscreen"))
        app.setFont(QFont(QStringLiteral("Segoe UI")));
#endif
    if(verification) {
        qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){
            const auto utf8=message.toUtf8();
            std::fprintf(stderr,"%s\n",utf8.constData());std::fflush(stderr);
        });
    }

    daymark::AppRuntime runtime;
    if (!runtime.start(verification,parser.value(QStringLiteral("data-dir")),parser.value(QStringLiteral("screenshot")),soakSeconds,parser.value(QStringLiteral("soak-report"))))
        return runtime.startupExitCode();
    return app.exec();
}
