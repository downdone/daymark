#include "app/AppRuntime.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <cstdio>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Daymark"));
    QCoreApplication::setOrganizationName(QStringLiteral("Daymark"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DAYMARK_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Daymark Windows V1 release candidate"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Check QML startup and exit without user data access.")});
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("Use an explicit local data directory."), QStringLiteral("directory")});
    parser.addOption({QStringLiteral("screenshot"),QStringLiteral("Save the synthetic smoke-test window image."),QStringLiteral("path")});
    parser.process(app);
    if(parser.isSet(QStringLiteral("smoke-test"))) {
        qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){
            const auto utf8=message.toUtf8();
            std::fprintf(stderr,"%s\n",utf8.constData());std::fflush(stderr);
        });
    }

    daymark::AppRuntime runtime;
    if (!runtime.start(parser.isSet(QStringLiteral("smoke-test")),parser.value(QStringLiteral("data-dir")),parser.value(QStringLiteral("screenshot"))))
        return runtime.startupExitCode();
    return app.exec();
}
