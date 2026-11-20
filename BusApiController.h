#ifndef BUSAPICONTROLLER_H
#define BUSAPICONTROLLER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVariantList>
#include <QString>
#include <QImage>
#include <QByteArray>
#include <QBuffer>
#include <QWebSocket>
#include <QSet>
#include "BusAudioEngine.h"
#include "SSEnvManager.h"

class BusApiController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stationName READ stationName NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList busList READ busList NOTIFY dataUpdated)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString arriveSoonBuses READ arriveSoonBuses NOTIFY arriveSoonBusesChanged)
    Q_PROPERTY(QString screenMode READ screenMode WRITE setScreenMode NOTIFY screenModeChanged)

    //env board telemetry
    Q_PROPERTY(int boardTemperature READ boardTemperature NOTIFY envTelemetryChanged)
    Q_PROPERTY(int boardHumidity READ boardHumidity NOTIFY envTelemetryChanged)
    Q_PROPERTY(bool boardConnected READ boardConnected NOTIFY envTelemetryChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryChanged)

private:
    QNetworkAccessManager *m_networkManager;
    QTimer *m_pollTimer;
    QString m_stationName = "Loading station name...";
    QVariantList m_busList;
    QString m_statusMessage = "Connecting to server";
    QString m_apiUrl;
    QWebSocket m_webSocket;
    QTimer m_wsReconnectTimer;
    QUrl m_daemonUrl;
    QString m_stationId = "12345";
    QString m_arriveSoonBuses = "";
    int m_arriveSoonStops;
    int m_arriveSoonMinutes;
    QString m_serviceKey;


    int m_batteryLevel = 100;
    int m_boardTemperature = 0;
    int m_boardHumidity = 0;
    int m_tconWriteMode = 4; //3 = 4-bit grayscale RLE (0x43), 4 = 1-bit B&W RLE (0x44)


    QString m_screenMode = "vertical";
    QString m_configPath;


    BusAudioEngine *m_audioEngine = nullptr;
    QSet<QString> m_announcedRoutes;

    SSEnvManager *m_envManager = nullptr;
    QTimer m_serialReconnectTimer;
    QString m_serialPort;
    int m_baudRate = 115200;

public:
    explicit BusApiController(QObject *parent = nullptr);

    int batteryLevel() const {return m_batteryLevel; }
    int boardTemperature() const { return m_boardTemperature; }
    int boardHumidity() const { return m_boardHumidity; }
    bool boardConnected() const { return m_envManager && m_envManager->connectYN(); }

    QString stationName() const {return m_stationName;}
    QVariantList busList() const {return m_busList;}
    QString statusMessage() const {return m_statusMessage;}
    QString screenMode() const { return m_screenMode; }
    QString arriveSoonBuses() const {return m_arriveSoonBuses; }
    int arriveSoonStops() const { return m_arriveSoonStops; }
    int arriveSoonMinutes() const { return m_arriveSoonMinutes; }

    void setScreenMode(const QString &mode);

    Q_INVOKABLE void startPolling(const QString &apiUrl, const QString &serviceKey, const QString &stationId, int intervalMs = 15000);
    Q_INVOKABLE void fetchArrivals();
    Q_INVOKABLE void processCapturedImage(const QImage &image);
    Q_INVOKABLE void connectToDaemon(const QString &wsUrl, const QString &stationId);
    //Q_INVOKABLE void sendImageToDaemon(const QImage &image);
    Q_INVOKABLE void connectToEnvBoard(const QString &portName, int baudRate, int tconWriteMode = 4);
    Q_INVOKABLE void sendImageToDaemon(const QImage &image); // Kept same name so QML needs 0 changes

    // Hardware controls callable directly from MenuWindow.qml
    Q_INVOKABLE void setBoardVolume(int volumeLevel);
    Q_INVOKABLE void requestBoardStatus();

private slots:
    void onApiReply(QNetworkReply *reply);
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessageReceived(const QString &message);
    void onEnvStateReceived(const SSEnv2State &state);
    void onEnvReconnectRequested();

signals:
    void dataUpdated();
    void statusChanged();
    void requestScreenCapture(const QString &filePath);
    void batteryChanged();
    void screenModeChanged();
    void envTelemetryChanged();
    void arriveSoonBusesChanged();
};

#endif // BUSAPICONTROLLER_H
