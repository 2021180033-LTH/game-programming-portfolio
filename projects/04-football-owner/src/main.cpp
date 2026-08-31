#include "application/game_controller.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("FootballOwner"));
    QCoreApplication::setApplicationName(QStringLiteral("FootballOwner"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.10.0"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDirectory)) {
        return 2;
    }

    football_owner::GameController controller(dataDirectory);
    (void)controller.initialize();
    QObject::connect(
        &controller,
        &football_owner::GameController::quitRequested,
        &application,
        &QCoreApplication::quit);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("game"), &controller);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("FootballOwner"), QStringLiteral("Main"));
    return application.exec();
}
