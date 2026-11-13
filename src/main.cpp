#include "main_window.h"
#include "app_version.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QImageReader>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QIcon>
#include <QFileInfo>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SureSort");
    QCoreApplication::setApplicationName("Sure Sort");QGuiApplication::setApplicationDisplayName("Sure Sort");
    QCoreApplication::setApplicationVersion(SURESORT_VERSION);
    Q_INIT_RESOURCE(branding);
    QApplication::setWindowIcon(QIcon(":/branding/suresort.ico"));
    QImageReader::setAllocationLimit(256);
    QCommandLineParser parser;
    parser.setApplicationDescription("Sure Sort · offline photo viewer and automatic keyboard sorting");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("folder", "Optional local photo folder");
    parser.addOption({"cache-mb", "Preview cache budget in MiB (default 256)", "MiB", "256"});
    parser.addOption({"smoke-test-ms", "Close automatically after N ms for launch verification", "ms"});
    parser.addOption({"session-db", "SQLite app profile path (folder progress is saved beside photos)", "path"});
    parser.addOption({"portable", "Keep app settings and local progress fallback in the application's data folder"});
    parser.process(app);
    bool valid = false;
    const qint64 mb = parser.value("cache-mb").toLongLong(&valid);
    if (!valid || mb < 0 || mb > 16384) parser.showHelp(2);
    const QDir appFolder(QCoreApplication::applicationDirPath());
    const bool portable = parser.isSet("portable") || QFileInfo::exists(appFolder.filePath("portable.ini"));
    const auto session = parser.isSet("session-db") ? parser.value("session-db")
        : portable ? appFolder.filePath("data/profile.sqlite") : QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("profile.sqlite");
    photocull::MainWindow window(mb * 1024 * 1024,nullptr,session);
    window.show();
    if (!parser.positionalArguments().isEmpty()) window.openFolder(parser.positionalArguments().first());
    if (parser.isSet("smoke-test-ms")) {
        const int ms = parser.value("smoke-test-ms").toInt(&valid);
        if (!valid || ms < 1) parser.showHelp(2);
        QTimer::singleShot(ms, &app, &QApplication::quit);
    }
    const int result = app.exec();
    window.shutdown(); // No GUI-thread drain during ordinary navigation/folder changes.
    return result;
}
