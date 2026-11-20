#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QDir>
#include "BusApiController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("SSNS");
    app.setApplicationName("BIT View");

    // Load config.ini from the executable's directory
    QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath("config.ini");
    QSettings settings(configPath, QSettings::IniFormat);

    QString apiUrl = settings.value("Network/BisApiUrl", "https://example.com").toString();
    QString serviceKey = settings.value("Network/ServiceKey", "").toString();
    QString daemonWsUrl = settings.value("Network/DaemonWsUrl", "ws://127.0.0.1:3000/ws").toString();
    QString stationId = settings.value("Terminal/StationId", "220000112").toString();
    int pollInterval = settings.value("Terminal/PollIntervalMs", 30000).toInt();


    QString serialPort = settings.value("Serial/PortName", "COM3").toString();
    int baudRate = settings.value("Serial/BaudRate", 115200).toInt();
    int tconMode = settings.value("Serial/TconWriteMode", 3).toInt();

    BusApiController busController;
    //busController.startPolling("https://apis.data.go.kr/6410000/busarrivalservice/v2/getBusArrivalListv2", 30000);
    busController.startPolling(apiUrl, serviceKey, stationId, pollInterval);
    //busController.connectToDaemon(daemonWsUrl, stationId);
    busController.connectToEnvBoard(serialPort, baudRate, tconMode);

    QQmlApplicationEngine engine;

     //expose the pointer to QML under the variable name "busController"
    engine.rootContext()->setContextProperty("busController", &busController);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("BitTestQt", "Main");

    return QGuiApplication::exec();
}
