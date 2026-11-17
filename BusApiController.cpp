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
    m_wsReconnectTimer.setSingleShot(true);
    m_wsReconnectTimer.setInterval(5000);
    connect(&m_wsReconnectTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "Attempting WebSocket reconnect to: " << m_daemonUrl.toString();
        m_webSocket.open(m_daemonUrl);
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

void BusApiController::sendImageToDaemon(const QImage &image) {
    if (image.isNull()) {
        qWarning() << "Cannot send null image";
        return;
    }

    if (m_webSocket.state() != QAbstractSocket::ConnectedState){
        qWarning() << "Websocket is not connected. Cannot send image";
        return;
    }

    //process on background thread START
    // Capture stationId by value so the background thread never touches 'this'
    QString stationId = m_stationId;

    auto future = QtConcurrent::run([this, image, stationId]() {
        QByteArray pngBytes;
        QBuffer buffer(&pngBytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");

        QString base64String = QString::fromLatin1(pngBytes.toBase64());

        QJsonObject payload;
        payload["station_id"] = stationId;
        payload["image_base64"] = base64String;

        QString jsonString = QString::fromUtf8(
            QJsonDocument(payload).toJson(QJsonDocument::Compact)
            );

        // Safely hop back to the main thread to send over QWebSocket
        QMetaObject::invokeMethod(this, [this, jsonString]() {
            if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
                qint64 bytesSent = m_webSocket.sendTextMessage(jsonString);
                qDebug() << "Background-encoded QHD frame sent:" << bytesSent << "bytes";
            }
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);

    //background thread process END

    //processing on ui/main thread START
    //encode image to png byte in RAM
    // QByteArray pngBytes;
    // QBuffer buffer(&pngBytes);
    // buffer.open(QIODevice::WriteOnly);
    // image.save(&buffer, "PNG");


    // //convert to base64 string
    // QString base64String = QString::fromLatin1(pngBytes.toBase64());

    // //construct JSON to send to daemon
    // QJsonObject payload;
    // payload["station_id"] = m_stationId;
    // payload["image_base64"] = base64String;

    // QString jsonString = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    // //send to websocket
    // qint64 bytesSent = m_webSocket.sendTextMessage(jsonString);
    // qDebug() << "Sent QHD PNG payload over websocket: " << bytesSent << " bytes";
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
        QVariantMap busMap;
        QJsonValue routeVal = item["routeName"];
        busMap["routeNumber"] = routeVal.isDouble()
                                    ? QString::number(routeVal.toInt())
                                    : routeVal.toString();

        busMap["destination"] = item["routeDestName"].toString();
        busMap["etaMinutes"] = item["predictTimeSec1"].toInt(-1);
        busMap["remainingStops"] = item["locationNo1"].toInt(-1);

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

    // if (listVal.isArray()) {
    //     QJsonArray arrivalArray = listVal.toArray(); //msgBody["busArrivalList"].toArray();



    //     for (const QJsonValue &val : arrivalArray){
    //         QJsonObject item = val.toObject();

    //         QVariantMap busMap;

    //         QJsonValue routeVal = item["routeName"];
    //         busMap["routeNumber"] = routeVal.isDouble()
    //                                     ? QString::number(routeVal.toInt())
    //                                     : routeVal.toString();

    //         busMap["destination"] = item["routeDestName"].toString();

    //         // First Bus Info
    //         busMap["etaMinutes"] = item["predictTimeSec1"].toInt(-1);
    //         busMap["remainingStops"] = item["locationNo1"].toInt(-1);

    //         updatedList.append(busMap);
    //     }
    // } else if (listVal.isObject()) {
    //     QJsonObject item = listVal.toObject();

    //     QVariantMap busMap;

    //     QJsonValue routeVal = item["routeName"];
    //     busMap["routeNumber"] = routeVal.isDouble()
    //                                 ? QString::number(routeVal.toInt())
    //                                 : routeVal.toString();

    //     busMap["destination"] = item["routeDestName"].toString();

    //     // First Bus Info
    //     busMap["etaMinutes"] = item["predictTimeSec1"].toInt(-1);
    //     busMap["remainingStops"] = item["locationNo1"].toInt(-1);

    //     updatedList.append(busMap);
    // }

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

        if ((etaSec > 0 && etaSec <= 90) || stops < 5) {
            currentlyArrivingSet.insert(route);
            allCurrentlyArriving.append({route, bus["busType"].toString()});
            qDebug() << "Arriving soon bus: " << route << " " << bus["busType"].toString(); // Should print QSize(2560, 1440)
        }
    }

    // Speak ALL currently arriving buses whenever the set changes
    // (e.g. {} -> {7}, {7} -> {7, 3}, or {7, 3} -> {3})
    if (!currentlyArrivingSet.isEmpty() && currentlyArrivingSet != m_announcedRoutes) {
        if (m_audioEngine) {
            m_audioEngine->playArriveSoonMessage(allCurrentlyArriving);
        }
    }

    // Save the current snapshot for the next poll comparison
    m_announcedRoutes = currentlyArrivingSet;

    emit dataUpdated();
    emit statusChanged();
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