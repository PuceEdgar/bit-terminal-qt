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

class BusApiController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stationName READ stationName NOTIFY dataUpdated)
    Q_PROPERTY(QVariantList busList READ busList NOTIFY dataUpdated)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryChanged)
    Q_PROPERTY(QString screenMode READ screenMode WRITE setScreenMode NOTIFY screenModeChanged)

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
    QString m_serviceKey;
    int m_batteryLevel = 100;
    QString m_screenMode = "vertical";
    QString m_configPath;
    BusAudioEngine *m_audioEngine = nullptr;
    QSet<QString> m_announcedRoutes;

public:
    explicit BusApiController(QObject *parent = nullptr);

    int batteryLevel() const {return m_batteryLevel; }

    QString stationName() const {return m_stationName;}
    QVariantList busList() const {return m_busList;}
    QString statusMessage() const {return m_statusMessage;}
    QString screenMode() const { return m_screenMode; }

    void setScreenMode(const QString &mode);

    Q_INVOKABLE void startPolling(const QString &apiUrl, const QString &serviceKey, const QString &stationId, int intervalMs = 15000);
    Q_INVOKABLE void fetchArrivals();
    Q_INVOKABLE void processCapturedImage(const QImage &image);
    Q_INVOKABLE void connectToDaemon(const QString &wsUrl, const QString &stationId);
    Q_INVOKABLE void sendImageToDaemon(const QImage &image);

private slots:
    void onApiReply(QNetworkReply *reply);
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessageReceived(const QString &message);

signals:
    void dataUpdated();
    void statusChanged();
    void requestScreenCapture(const QString &filePath);
    void batteryChanged();
    void screenModeChanged();
};

#endif // BUSAPICONTROLLER_H
