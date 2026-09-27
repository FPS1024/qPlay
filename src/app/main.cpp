#include "player_engine.h"
#include "video/mpv_video_item.h"

#include <QFileInfo>
#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QtQml>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    application.setApplicationName(QStringLiteral("qPlay"));
    application.setApplicationVersion(QStringLiteral("0.1.1"));
    application.setOrganizationName(QStringLiteral("qPlay"));
    application.setOrganizationDomain(QStringLiteral("qplay.local"));

    QCommandLineParser commandLine;
    commandLine.setApplicationDescription(QStringLiteral("qPlay media player"));
    commandLine.addHelpOption();
    commandLine.addVersionOption();
    commandLine.addPositionalArgument(
        QStringLiteral("media"),
        QStringLiteral("Media file or URL to open."),
        QStringLiteral("[media...]"));
    commandLine.process(application);

    qmlRegisterType<QuarkTV::Video::MpvVideoItem>("QuarkTV", 1, 0, "MpvVideoItem");
    qmlRegisterUncreatableMetaObject(QuarkTV::staticMetaObject,
                                     "QuarkTV",
                                     1,
                                     0,
                                     "PlaybackState",
                                     QStringLiteral("PlaybackState is a namespace."));

    QuarkTV::App::PlayerEngine player;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("player"), &player);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    const QStringList mediaArguments = commandLine.positionalArguments();
    if (!mediaArguments.isEmpty()) {
        const QString firstMedia = mediaArguments.constFirst();
        QTimer::singleShot(0, &player, [&player, firstMedia] {
            player.openPath(firstMedia);
        });
        for (qsizetype index = 1; index < mediaArguments.size(); ++index) {
            player.addToPlaylist(
                QUrl::fromLocalFile(QFileInfo(mediaArguments.at(index)).absoluteFilePath()),
                QFileInfo(mediaArguments.at(index)).fileName());
        }
    }

    return application.exec();
}
