#include "BusApiController.h"
#include "BusAudioEngine.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QDebug>
#include <QBuffer>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

BusApiController::BusApiController(QObject *parent) : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_pollTimer(new QTimer(this)) {

    m_configPath = QDir(QCoreApplication::applicationDirPath()).filePath("config.ini");
    QSettings settings(m_configPath, QSettings::IniFormat);
    m_screenMode = settings.value("Display/ScreenMode", "vertical").toString();
    m_stationName = settings.value("Terminal/StationName", "Unknown").toString();
    m_arriveSoonMinutes = settings.value("Terminal/ArriveSoonMinutes", 3).toInt();
    m_arriveSoonStops = settings.value("Terminal/ArriveSoonStops", 2).toInt();
    QString soundDir = QDir(QCoreApplication::applicationDirPath()).filePath("sounds");
    m_audioEngine = new BusAudioEngine(soundDir, this);

    //connect network response signal
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &BusApiController::onApiReply);

    //connect timer for periodic polling
    connect(m_pollTimer, &QTimer::timeout, this, &BusApiController::fetchArrivals);

    //websocket hooks
    connect(&m_webSocket, &QWebSocket::connected, this, &BusApiController::onWsConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &BusApiController::onWsDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &BusApiController::onWsTextMessageReceived);

    //5 second auto reconnect timer
    // m_wsReconnectTimer.setSingleShot(true);
    // m_wsReconnectTimer.setInterval(5000);
    // connect(&m_wsReconnectTimer, &QTimer::timeout, this, [this]() {
    //     qDebug() << "Attempting WebSocket reconnect to: " << m_daemonUrl.toString();
    //     m_webSocket.open(m_daemonUrl);
    // });

    m_envManager = new SSEnvManager(this);
    connect(m_envManager, &SSEnvManager::stateReceived, this, &BusApiController::onEnvStateReceived);
    connect(m_envManager, &SSEnvManager::reconnectRequested, this, &BusApiController::onEnvReconnectRequested);
    connect(m_envManager, &SSEnvManager::disconnected, this, [this]() {
        emit envTelemetryChanged();
        if(!m_serialReconnectTimer.isActive()){
            m_serialReconnectTimer.start();
        }
    });

    // 5-second auto-reconnect timer for serial port
    m_serialReconnectTimer.setSingleShot(true);
    m_serialReconnectTimer.setInterval(5000);
    connect(&m_serialReconnectTimer, &QTimer::timeout, this, [this]() {
        if(!m_serialPort.isEmpty() && !m_envManager->connectYN()){
            qDebug() << "Attempting serial reconnect to: " << m_serialPort;
            m_envManager->connectProc(m_serialPort, m_baudRate);
            emit envTelemetryChanged();
        }
    });
}

void BusApiController::connectToDaemon(const QString &wsUrl, const QString &stationId){
    m_daemonUrl = QUrl(wsUrl);
    m_stationId = stationId;
    m_webSocket.open(m_daemonUrl);
}

void BusApiController::onWsConnected(){
    qDebug() << "Connected to c# daemon websocket";
    m_wsReconnectTimer.stop();
}

void BusApiController::onWsDisconnected(){
    qDebug() << "Daemon WS closed. Reconnecting in 5 seconds...";
    m_wsReconnectTimer.start();
}

void BusApiController::onWsTextMessageReceived(const QString &message){
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());

    if (!doc.isObject()) return;

    QJsonObject telemetry = doc.object();
    if (telemetry.contains("battery")) {
        m_batteryLevel = telemetry["battery"].toInt();
        emit batteryChanged();
        qDebug() << "Updated battery telemetry: " << m_batteryLevel;
    }
}


void BusApiController::startPolling(const QString &apiUrl, const QString &serviceKey, const QString &stationId, int intervalMs) {
    m_apiUrl = apiUrl;
    m_serviceKey = serviceKey;
    m_stationId = stationId;
    fetchArrivals(); //immediate first call
    m_pollTimer->start(intervalMs);
}

void BusApiController::fetchArrivals() {
    if (m_apiUrl.isEmpty()) return;

    QUrl url(m_apiUrl);

    //define query params
    QUrlQuery query;
    query.addQueryItem("stationId", m_stationId);
    query.addQueryItem("serviceKey", m_serviceKey);
    query.addQueryItem("format", "json");

    //attach query params to url
    // Use this ONLY if your serviceKey already contains '%' characters
    url.setQuery(query.toString(QUrl::FullyEncoded), QUrl::StrictMode);


    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(5000);

    m_networkManager->get(request);
}

void BusApiController::onApiReply(QNetworkReply *reply){

    m_arriveSoonBuses = "";
    if (reply->error() != QNetworkReply::NoError) {
        m_statusMessage = "Network Error: " + reply->errorString();
        emit statusChanged();
        reply->deleteLater();
        return;
    }

    QByteArray rawData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(rawData);

    if (!doc.isObject()){
        m_statusMessage = "Invalid JSON response";
        emit statusChanged();
        reply->deleteLater();
        return;
    }

    //navigate to response
    QJsonObject rootObj = doc.object();
    QJsonObject responseObj = rootObj["response"].toObject();

    //check api response code
    QJsonObject msgHeader = responseObj["msgHeader"].toObject();
    int resultCode = msgHeader["resultCode"].toInt(-1);

    if (resultCode != 0){
        m_statusMessage = "API Error: " + msgHeader["resultMessage"].toString();
        emit statusChanged();
        reply->deleteLater();
        return;
    }

    // 3. Navigate to "msgBody" -> "busArrivalList"
    QJsonObject msgBody = responseObj["msgBody"].toObject();
    QJsonValue listVal = msgBody["busArrivalList"];
    //parse json
    QVariantList updatedList;


    auto parseBusItem = [](const QJsonObject &item) -> QVariantMap {

        //qDebug() << item;

        //qDebug() << "Route name: " << item["routeName"] << " Destination: " << item["routeDestName"].toString() << " ETA minutes: " << item["predictTime1"] << " Remaining Stops: " << item["locationNo1"];
        QVariantMap busMap;
        QJsonValue routeVal = item["routeName"];
        busMap["routeNumber"] = routeVal.isDouble()
                                    ? QString::number(routeVal.toInt())
                                    : routeVal.toString();

        busMap["destination"] = item["routeDestName"].toString();
        busMap["etaMinutes"] = item["predictTime1"].toInt();
        busMap["remainingStops"] = item["locationNo1"].toInt();

        // Map Gyeonggi-do routeTypeCd to your WAV file prefix (if matching WAV exists)
        int typeCd = item["routeTypeCd"].toInt(-1);
        QString busType;
        if (typeCd == 11) busType = QStringLiteral("직행좌석");
        else if (typeCd == 12) busType = QStringLiteral("좌석");
        else if (typeCd == 13) busType = QStringLiteral("일반");
        else if (typeCd == 30) busType = QStringLiteral("마을");
        else if (typeCd == 14 || typeCd == 16) busType = QStringLiteral("광역");
        busMap["busType"] = busType;

        return busMap;
    };

    if (listVal.isArray()) {
        const QJsonArray arrivalArray = listVal.toArray();
        for (const QJsonValue &val : arrivalArray) {
            updatedList.append(parseBusItem(val.toObject()));
        }
    } else if (listVal.isObject()) {
        updatedList.append(parseBusItem(listVal.toObject()));
    }


    m_busList = updatedList;
    m_statusMessage = "Online";

    // Collect ALL buses currently in the arriving-soon window
    QList<QPair<QString, QString>> allCurrentlyArriving;
    QSet<QString> currentlyArrivingSet;

    for (const QVariant &v : std::as_const(m_busList)) {
        QVariantMap bus = v.toMap();
        QString route = bus["routeNumber"].toString();
        int etaSec = bus["etaMinutes"].toInt();
        int stops = bus["remainingStops"].toInt();

        if ((etaSec > 0 && etaSec <= m_arriveSoonMinutes) || stops < m_arriveSoonStops) {
            currentlyArrivingSet.insert(route);
            allCurrentlyArriving.append({route, bus["busType"].toString()});
            m_arriveSoonBuses += route + " ";

            qDebug() << "Arriving soon bus: " << route << " " << bus["busType"].toString(); // Should print QSize(2560, 1440)
        }
    }

    qDebug() << "m_arriveSoonBuses: " << m_arriveSoonBuses;
    // Speak ALL currently arriving buses whenever the set changes
    // (e.g. {} -> {7}, {7} -> {7, 3}, or {7, 3} -> {3})
    if (!currentlyArrivingSet.isEmpty() && currentlyArrivingSet != m_announcedRoutes) {
        qDebug() << "audio engine: ";
        qDebug() << m_audioEngine;
        if (m_audioEngine) {
            m_audioEngine->playArriveSoonMessage(allCurrentlyArriving);
        }
    }

    // Save the current snapshot for the next poll comparison
    m_announcedRoutes = currentlyArrivingSet;

    emit dataUpdated();
    emit statusChanged();
    emit arriveSoonBusesChanged();
    reply->deleteLater();
}

void BusApiController::processCapturedImage(const QImage &image) {
    if (image.isNull()) {
        qWarning() << "Received empty image from QML!";
        return;
    }

    // Optional: Convert to RGB888 (24-bit without alpha channel) if your hardware dislikes transparency
    QImage opaqueImage = image.convertToFormat(QImage::Format_RGB888);

    // 1. Convert to compressed PNG or JPEG QByteArray in memory
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    opaqueImage.save(&buffer, "PNG"); // Change to "JPG" if smaller payload is needed

    qDebug() << "Captured Image Resolution:" << opaqueImage.size(); // Should print QSize(2560, 1440)
    qDebug() << "Encoded Byte Array Size:" << byteArray.size() << "bytes";

    // 2. If your hardware needs raw uncompressed pixel bytes instead of PNG/JPG:
    // QByteArray rawPixels(reinterpret_cast<const char*>(opaqueImage.constBits()), opaqueImage.sizeInBytes());

    // TODO: Send 'byteArray' over QTcpSocket / Serial / HTTP POST to your hardware
}

void BusApiController::setScreenMode(const QString &mode) {

    if (m_screenMode == mode) {
        return;
    }

    m_screenMode = mode;

    //Write to config.ini
    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.setValue("Display/ScreenMode", m_screenMode);
    settings.sync(); //flush to disk

    emit screenModeChanged();
}

void BusApiController::connectToEnvBoard(const QString &portName, int baudRate, int tconWriteMode)
{
    m_serialPort = portName;
    m_baudRate = baudRate;
    m_tconWriteMode = tconWriteMode;

    m_envManager->connectProc(m_serialPort, m_baudRate);
    emit envTelemetryChanged();

    if (!m_envManager->connectYN()) {
        m_serialReconnectTimer.start();
    }
}

void BusApiController::onEnvReconnectRequested()
{
    if (!m_serialPort.isEmpty()) {
        m_envManager->connectProc(m_serialPort, m_baudRate);
        emit envTelemetryChanged();
    }
}

void BusApiController::onEnvStateReceived(const SSEnv2State &state)
{
    if (m_batteryLevel != state.battPercent) {
        m_batteryLevel = state.battPercent;
        emit batteryChanged();
    }
    m_boardTemperature = state.temperature1;
    m_boardHumidity = state.humidity;
    emit envTelemetryChanged();
}

void BusApiController::setBoardVolume(int volumeLevel)
{
    if (m_envManager) {
        m_envManager->sendVolumeSet(qBound(0, volumeLevel, 10));
    }
}

void BusApiController::requestBoardStatus()
{
    if (m_envManager) {
        m_envManager->sendStatusRequest();
    }
}

void BusApiController::sendImageToDaemon(const QImage &image)
{
    if (image.isNull()) {
        qWarning() << "Cannot send null image";
        return;
    }

    if (!m_envManager || !m_envManager->connectYN()) {
        qWarning() << "ENV serial board is not connected. Cannot send TCON frame.";
        return;
    }

    int writeMode = m_tconWriteMode;
 QString stationId = m_stationId;
    // Convert QImage to 8-bit grayscale on a worker thread so UI stays 60 FPS
    auto future = QtConcurrent::run([this, image, stationId, writeMode]() {
     //QImage flippedImage = image.flipped(Qt::Horizontal);
     QImage flippedImage = image.mirrored(true, false);
     QImage gray = flippedImage.convertToFormat(QImage::Format_Grayscale8);
        //QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
        int w = gray.width();
        int h = gray.height();

        // Copy row-by-row to strip any 32-bit stride alignment padding from QImage
        QByteArray rawPixels;
        rawPixels.reserve(w * h);
        for (int y = 0; y < h; ++y) {
            rawPixels.append(reinterpret_cast<const char *>(gray.constScanLine(y)), w);
        }

        // Hop back to main thread to safely enqueue onto SSEnvManager's Tx queue
        QMetaObject::invokeMethod(this, [this, rawPixels, w, h, writeMode]() {
            if (!m_envManager || !m_envManager->connectYN()) return;

            if (writeMode == 4) {
                m_envManager->tconImageWrite4(rawPixels, 0, 0, w, h);
            } else {
                m_envManager->tconImageWrite3(rawPixels, 0, 0, w, h);
            }
            qDebug() << "Queued local TCON frame (" << w << "x" << h << ") mode:" << writeMode;
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}