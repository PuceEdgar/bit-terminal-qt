#include "SSEnvManager.h"
#include "BitConvertUtils.h"
#include <QProcess>
#include <QDebug>
#include <stdexcept>

SSEnvManager::SSEnvManager(QObject *parent)
    : QObject(parent),
    m_dtTime(new QTimer(this)),
    m_dtStatusInfo(new QTimer(this)),
    m_dtStatusResponse(new QTimer(this))
{
    try {
        m_dtTime->setInterval(100);
        connect(m_dtTime, &QTimer::timeout, this, &SSEnvManager::onTimeTick);

        m_dtStatusInfo->setInterval(30 * 1000);
        connect(m_dtStatusInfo, &QTimer::timeout, this, &SSEnvManager::onStatusInfoTick);

        m_dtStatusResponse->setInterval(60 * 1000);
        connect(m_dtStatusResponse, &QTimer::timeout, this, &SSEnvManager::onStatusResponseTick);
        m_dtStatusResponse->start();
    } catch (const std::exception &ex) {
        qWarning() << "SSEnvManager ctor exception:" << ex.what();
    } catch (...) {
    }
}

SSEnvManager::~SSEnvManager()
{
    disconnectProc();
}

void SSEnvManager::connectProc(const QString &portName, int baudRate)
{
    try {
        if (!m_env2) {
            m_env2 = new SSEnvModules(this);
            connect(m_env2, &SSEnvModules::dataReceived, this, &SSEnvManager::onModuleDataReceived);
            connect(m_env2, &SSEnvModules::disconnected, this, &SSEnvManager::onModuleDisconnected);
            connect(m_env2, &SSEnvModules::serialPortError, this, &SSEnvManager::onModuleSpError);
        }

        bool result = m_env2->initProc(portName, baudRate);
        qInfo() << "SerialPort InitProc result:" << result;

        if (!m_dtTime->isActive()) m_dtTime->start();
        if (!m_dtStatusInfo->isActive()) m_dtStatusInfo->start();

        if (result) {
            sendStatusRequest();
        }
    } catch (const std::exception &ex) {
        qWarning() << "connectProc exception:" << ex.what();
    } catch (...) {
    }
}

void SSEnvManager::disconnectProc()
{
    try {
        if (!m_env2) {
            qWarning() << "SerialPort is not connected.";
            return;
        }
        m_env2->doFinal();
    } catch (const std::exception &ex) {
        qWarning() << "disconnectProc exception:" << ex.what();
    } catch (...) {
    }
}

bool SSEnvManager::connectYN() const
{
    try {
        return m_env2 && m_env2->isOpen();
    } catch (...) {
        return false;
    }
}

void SSEnvManager::onModuleSpError() { emit serialPortError(); }

void SSEnvManager::onModuleDisconnected()
{
    try {
        qInfo() << "Serial connection closed.";
        emit disconnected();
    } catch (...) {
    }
}

void SSEnvManager::onTimeTick()
{
    try {
        tickTconImageTransfer();
    } catch (...) {
    }
}

void SSEnvManager::onStatusInfoTick()
{
    try {
        sendStatusRequest();
    } catch (...) {
    }
}

void SSEnvManager::onStatusResponseTick()
{
    try {
        if (m_processKillTimerSecondCount++ >= 2) {
            m_processKillTimerSecondCount = 0;
            qInfo() << "No status response received; attempting reconnect.";
            disconnectProc();
            emit reconnectRequested();
        }
    } catch (...) {
    }
}

void SSEnvManager::tickTconImageTransfer()
{
    try {
        if (m_bResponseTconOk) {
            if (!m_arrayTx.isEmpty()) {
                m_nNoResponseTime = 0;
                m_bResponseTconOk = false;
                m_nSendCnt++;

                QByteArray aTx = m_arrayTx.dequeue();
                qDebug() << "aTx size:" << aTx.size();

                if (m_env2) {
                    m_env2->sendData(aTx, 0, aTx.size());
                }
                qInfo() << "Tx Queue Count =" << m_arrayTx.size() << ", Length =" << aTx.size();
            }
        } else {
            if (++m_nNoResponseTime >= 600) { // 60 seconds (600 * 100ms)
                m_nNoResponseTime = 0;
                while (m_arrayTx.size() > 9) {
                    m_arrayTx.dequeue();
                    m_nTotalDiscardQue++;
                }

                m_bResponseTconOk = true;
                m_nResponseRestartCnt++;
                qWarning() << "Tx Forced Restart Count =" << m_nResponseRestartCnt
                           << "Send Count =" << m_nSendCnt
                           << "Discarded =" << m_nTotalDiscardQue;

                emit serialPortError();
            }
        }
    } catch (const std::exception &ex) {
        qWarning() << "tickTconImageTransfer exception:" << ex.what();
    } catch (...) {
    }
}

bool SSEnvManager::searchNewItem()
{
    if (m_arrayTx.isEmpty()) return false;
    if (m_arrayTx.size() == 1) return true;

    int txLen = m_arrayTx.head().size();
    while (m_arrayTx.size() > 1) {
        bool duplicateLenFound = false;
        for (int i = 1; i < m_arrayTx.size(); ++i) {
            if (m_arrayTx[i].size() == txLen) {
                duplicateLenFound = true;
                break;
            }
        }
        if (duplicateLenFound) {
            m_arrayTx.dequeue();
        } else {
            break;
        }
    }
    return !m_arrayTx.isEmpty();
}

void SSEnvManager::tconImageWrite(const QByteArray &bStr, int x, int y, int w, int h)
{
    try {
        if (bStr.size() > (2560 * 1440) / 4) {
            throw std::runtime_error("TCON write byte size exceeded.");
        }

        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));

        qint32 nSize = 3 + bStr.size() + 8;
        QByteArray byteSize = BitConvertUtils::fromInt32(nSize, true);
        bytes.append(byteSize[2]);
        bytes.append(byteSize[3]);

        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(byteSize[1]);
        bytes.append(static_cast<char>(SSEnv2OpcodeType::TconBitmapWrite));

        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(x), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(y), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(w), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(h), true));
        bytes.append(bStr);
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        m_arrayTx.enqueue(bytes);
    } catch (const std::exception &ex) {
        qWarning() << "tconImageWrite exception:" << ex.what();
    } catch (...) {
    }
}

void SSEnvManager::tconImageWrite3(const QByteArray &bStr, int x, int y, int w, int h)
{
    try {
        if (bStr.size() > (2560 * 1440)) {
            throw std::runtime_error("TCON write3 byte size exceeded.");
        }

        QByteArray compressed = rleEncode(bStr, w * h);

        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));

        qint32 nSize = 3 + compressed.size() + 8;
        QByteArray byteSize = BitConvertUtils::fromInt32(nSize, true);
        bytes.append(byteSize[2]);
        bytes.append(byteSize[3]);

        bytes.append(static_cast<char>(wfMode));
        bytes.append(byteSize[1]);
        bytes.append(static_cast<char>(SSEnv2OpcodeType::TconBitmapWrite3));

        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(x), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(y), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(w), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(h), true));
        bytes.append(compressed);
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        m_arrayTx.enqueue(bytes);
    } catch (const std::exception &ex) {
        qWarning() << "tconImageWrite3 exception:" << ex.what();
    } catch (...) {
    }
}

void SSEnvManager::tconImageWrite4(const QByteArray &bStr, int x, int y, int w, int h)
{
    try {
        if (bStr.size() > (2560 * 1440)) {
            throw std::runtime_error("TCON write4 byte size exceeded.");
        }

        QByteArray compressed = rleEncodeBW(bStr, w * h);

        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));

        qint32 nSize = 3 + compressed.size() + 8;
        QByteArray byteSize = BitConvertUtils::fromInt32(nSize, true);
        bytes.append(byteSize[2]);
        bytes.append(byteSize[3]);

        bytes.append(static_cast<char>(1));
        bytes.append(byteSize[1]);
        bytes.append(static_cast<char>(SSEnv2OpcodeType::TconBitmapWrite4));

        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(x), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(y), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(w), true));
        bytes.append(BitConvertUtils::fromInt16(static_cast<qint16>(h), true));
        bytes.append(compressed);
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        m_arrayTx.enqueue(bytes);
    } catch (const std::exception &ex) {
        qWarning() << "tconImageWrite4 exception:" << ex.what();
    } catch (...) {
    }
}

QByteArray SSEnvManager::rleEncode(const QByteArray &src, int pixelCount)
{
    QByteArray dst;
    int limit = qMin(pixelCount, src.size());
    dst.reserve(limit);

    int srcIdx = 0;
    while (srcIdx < limit) {
        quint8 color = static_cast<quint8>(src[srcIdx]) & 0xF0;
        quint8 length = 1;

        while ((srcIdx + length < limit) &&
               ((static_cast<quint8>(src[srcIdx + length]) & 0xF0) == color) &&
               (length < 255)) {
            ++length;
        }

        dst.append(static_cast<char>(length));
        dst.append(static_cast<char>(color));
        srcIdx += length;
    }
    return dst;
}

QByteArray SSEnvManager::rleEncodeBW(const QByteArray &src, int pixelCount)
{
    QByteArray dst;
    int limit = qMin(pixelCount, src.size());
    dst.reserve(limit);

    int srcIdx = 0;
    while (srcIdx < limit) {
        quint8 color = (static_cast<quint8>(src[srcIdx]) >= 0xC0) ? 0xFF : 0x00;
        quint8 length = 1;

        while ((srcIdx + length < limit) &&
               ((static_cast<quint8>(src[srcIdx + length]) >= 0xC0 ? 0xFF : 0x00) == color) &&
               (length < 255)) {
            ++length;
        }

        dst.append(static_cast<char>(length));
        dst.append(static_cast<char>(color));
        srcIdx += length;
    }
    return dst;
}

void SSEnvManager::sendStatusRequest()
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::StatusRequest));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        QString hexStr = BitConvertUtils::byteArrayToHexStr(bytes, QStringLiteral("-"));
        if (!m_env2) {
            qCritical() << "[StatusRequest] Env board not connected:" << hexStr;
        } else {
            m_arrayTx.enqueue(bytes);
        }
    } catch (...) {
    }
}

void SSEnvManager::sendDeviceControlRequest(quint8, quint8, int, int) {}

void SSEnvManager::sendVolumeRequest()
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::VolumeRequest));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendVolumeSet(int volumeLevel)
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(4, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::VolumeSet));
        bytes.append(static_cast<char>(volumeLevel));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendEnvBoardTimeRequest()
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardTimeRequest));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendEnvBoardTimeSet(const QString &dateTime14)
{
    try {
        if (dateTime14.length() != 14) return;
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3 + 14, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardTimeSet));
        bytes.append(dateTime14.toLatin1());
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendEnvBoardConfigRequest()
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardConfig));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendDcFanSet(int mode, int onTemp, int offTemp)
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3 + 3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardConfigFan));
        bytes.append(static_cast<char>(mode));
        bytes.append(static_cast<char>(onTemp));
        bytes.append(static_cast<char>(offTemp));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendHeaterSet(int mode, int onTemp, int offTemp)
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3 + 3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardConfigHeater));
        bytes.append(static_cast<char>(mode));
        bytes.append(static_cast<char>(onTemp));
        bytes.append(static_cast<char>(offTemp));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendLampSet(int mode, int onHour, int onMin, int offHour, int offMin)
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3 + 5, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardConfigLamp));
        bytes.append(static_cast<char>(mode));
        bytes.append(static_cast<char>(onHour));
        bytes.append(static_cast<char>(onMin));
        bytes.append(static_cast<char>(offHour));
        bytes.append(static_cast<char>(offMin));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendScreenDeviceSet(int mode, int onHour, int onMin, int offHour, int offMin)
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3 + 5, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvBoardConfigScreen));
        bytes.append(static_cast<char>(mode));
        bytes.append(static_cast<char>(onHour));
        bytes.append(static_cast<char>(onMin));
        bytes.append(static_cast<char>(offHour));
        bytes.append(static_cast<char>(offMin));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendPeriodAndShockSet(int periodVal, int shockVal, int luminanceVal, int illuminationVal)
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3 + 4, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvPeriodEtcSet));
        bytes.append(static_cast<char>(periodVal));
        bytes.append(static_cast<char>(shockVal));
        bytes.append(static_cast<char>(luminanceVal));
        bytes.append(static_cast<char>(illuminationVal));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::sendPeriodShockLuminanceRequest()
{
    try {
        QByteArray bytes;
        bytes.append(static_cast<char>(STX));
        bytes.append(static_cast<char>(STX));
        bytes.append(BitConvertUtils::fromInt16(3, true));
        bytes.append(static_cast<char>(SSEnv2MachineType::EnvBoard));
        bytes.append(static_cast<char>(SSEnv2EventType::Default));
        bytes.append(static_cast<char>(SSEnv2OpcodeType::EnvPeriodRequest));
        bytes.append(static_cast<char>(BitConvertUtils::createCheckSum(bytes, bytes.size())));

        if (m_env2) m_arrayTx.enqueue(bytes);
    } catch (...) {
    }
}

void SSEnvManager::updateEnvData(const SSEnv2State &newState)
{
    m_envData = newState;
}

void SSEnvManager::onModuleDataReceived(const QByteArray &receiveData)
{
    try {
        SSEnv2Protocol data = parseEnv2Protocol(receiveData);
        if (data.resultCd != 1) {
            qInfo() << "[" << data.resultCd << "]" << data.resultMsg;
            return;
        }

        switch (data.opGbn) {
        case SSEnv2OpcodeType::EnvBoardTimeRequest: {
            SSEnv2BoardState item = convertBoardStateInfo(data.data);
            if (!item.mBoardDateTime.isEmpty()) emit boardStateReceived(item);
            break;
        }
        case SSEnv2OpcodeType::StatusResponse: {
            SSEnv2State item = recvStatusInfo(data.data);
            if (item.nVersion != 0 || data.data.size() >= 22) {
                emit stateReceived(item);
                if (data.eventGbn != SSEnv2EventType::ShockEvent &&
                    data.eventGbn != SSEnv2EventType::DoorEvent) {
                    updateEnvData(item);
                }
            }
            m_processKillTimerSecondCount = 0;
            break;
        }
        case SSEnv2OpcodeType::EnvBoardConfig: {
            if (data.data.size() == 16) {
                emit configReceived(recvEnvBoardConfig(data.data));
            }
            break;
        }
        case SSEnv2OpcodeType::EnvPeriodRequest:
        case SSEnv2OpcodeType::EnvPeriodEtcSet: {
            if (data.data.size() == 4) {
                emit sensorReceived(convertShockInfo(data.data));
            }
            break;
        }
        case SSEnv2OpcodeType::PcPowerOffEvent: {
            if (!data.data.isEmpty() && static_cast<quint8>(data.data[0]) == 0xA5) {
                qInfo() << "[PowerOff] Attempting shutdown. Count:" << m_retryPowerOffCount++;
#ifdef Q_OS_WIN
                QProcess::startDetached(QStringLiteral("shutdown"), {QStringLiteral("-s"), QStringLiteral("-f"), QStringLiteral("-t"), QStringLiteral("10")});
#else
                QProcess::startDetached(QStringLiteral("sudo"), {QStringLiteral("shutdown"), QStringLiteral("-h"), QStringLiteral("now")});
#endif
            }
            break;
        }
        case SSEnv2OpcodeType::TconBitmapWrite:
        case SSEnv2OpcodeType::TconBitmapWrite1:
        case SSEnv2OpcodeType::TconBitmapWrite2:
        case SSEnv2OpcodeType::TconBitmapWrite3:
        case SSEnv2OpcodeType::TconBitmapWrite4: {
            quint8 ack = data.data.isEmpty() ? 0 : static_cast<quint8>(data.data[0]);
            qInfo() << "[UART] Rx TCON Count =" << m_arrayTx.size() << ", Ack =" << ack;
            break;
        }
        default:
            break;
        }

        m_bResponseTconOk = true;
    } catch (const std::exception &ex) {
        qWarning() << "onModuleDataReceived exception:" << ex.what();
    } catch (...) {
    }
}

SSEnv2Protocol SSEnvManager::parseEnv2Protocol(const QByteArray &bytes)
{
    SSEnv2Protocol item;
    try {
        if (bytes.size() < 8) {
            item.resultCd = -1;
            item.resultMsg = QStringLiteral("Packet too short");
            return item;
        }

        item.stx1 = static_cast<quint8>(bytes[0]);
        item.stx2 = static_cast<quint8>(bytes[1]);
        item.size = bytes.mid(2, 2);

        int sz = BitConvertUtils::toInt16(item.size) - 3;
        if (sz < 0 || bytes.size() < 7 + sz + 1) {
            item.resultCd = -1;
            item.resultMsg = QStringLiteral("Invalid payload size");
            return item;
        }

        item.machineCode = static_cast<quint8>(bytes[4]);
        item.mcGbn = static_cast<SSEnv2MachineType>(item.machineCode);
        item.ctrlOpcode = static_cast<quint8>(bytes[5]);
        item.eventGbn = static_cast<SSEnv2EventType>(item.ctrlOpcode);
        item.opcode = static_cast<quint8>(bytes[6]);
        item.opGbn = static_cast<SSEnv2OpcodeType>(item.opcode);
        item.data = bytes.mid(7, sz);
        item.checkSum = static_cast<quint8>(bytes[bytes.size() - 1]);

        item.resultCd = 1;
        item.resultMsg = QStringLiteral("변환완료");
        return item;
    } catch (const std::exception &ex) {
        item.resultCd = -1;
        item.resultMsg = QString::fromUtf8(ex.what());
        return item;
    } catch (...) {
        item.resultCd = -1;
        item.resultMsg = QStringLiteral("Exception");
        return item;
    }
}

SSEnv2State SSEnvManager::recvStatusInfo(const QByteArray &bytes)
{
    SSEnv2State item;
    try {
        if (bytes.size() < 22) return item;

        item.nVersion     = BitConvertUtils::toInt16(bytes.mid(0, 2), false);
        item.mainVoltage  = static_cast<float>(BitConvertUtils::toInt16(bytes.mid(2, 2), false)) / 100.0f;
        item.mainCurrent  = static_cast<float>(BitConvertUtils::toInt16(bytes.mid(4, 2), false)) / 100.0f;
        item.battVoltage  = static_cast<float>(BitConvertUtils::toInt16(bytes.mid(6, 2), false)) / 100.0f;
        item.battCurrent  = static_cast<float>(BitConvertUtils::toInt16(bytes.mid(8, 2), false)) / 100.0f;
        item.panlVoltage  = static_cast<float>(BitConvertUtils::toInt16(bytes.mid(10, 2), false)) / 100.0f;
        item.panlCurrent  = static_cast<float>(BitConvertUtils::toInt16(bytes.mid(12, 2), false)) / 100.0f;

        item.temperature1 = static_cast<qint8>(bytes[14]);
        item.humidity     = static_cast<qint8>(bytes[15]);
        item.luminance    = static_cast<qint8>(bytes[16]);
        item.shock1       = static_cast<qint8>(bytes[17]);
        item.door         = static_cast<qint8>(bytes[18]);
        item.illumination = static_cast<qint8>(bytes[19]);
        item.temperature2 = static_cast<qint8>(bytes[20]);
        item.battPercent  = static_cast<qint8>(bytes[21]);
        if (bytes.size() >= 24) {
            item.remainBat = BitConvertUtils::toInt16(bytes.mid(22, 2), false);
        }
    } catch (const std::exception &ex) {
        qWarning() << "recvStatusInfo exception:" << ex.what();
    } catch (...) {
    }
    return item;
}

SSEnv2Config SSEnvManager::recvEnvBoardConfig(const QByteArray &bytes)
{
    SSEnv2Config item;
    try {
        if (bytes.size() != 16) return item;

        item.nFanMode       = static_cast<quint8>(bytes[0]);
        item.nFanOnTemp     = static_cast<quint8>(bytes[1]);
        item.nFanOffTemp    = static_cast<quint8>(bytes[2]);

        item.nHeaterMode    = static_cast<quint8>(bytes[3]);
        item.nHeaterOnTemp  = static_cast<quint8>(bytes[4]);
        item.nHeaterOffTemp = static_cast<quint8>(bytes[5]);

        item.nLampMode      = static_cast<quint8>(bytes[6]);
        item.nLampOnHour    = static_cast<quint8>(bytes[7]);
        item.nLampOnMin     = static_cast<quint8>(bytes[8]);
        item.nLampOffHour   = static_cast<quint8>(bytes[9]);
        item.nLampOffMin    = static_cast<quint8>(bytes[10]);

        item.nScreenMode    = static_cast<quint8>(bytes[11]);
        item.nScreenOnHour  = static_cast<quint8>(bytes[12]);
        item.nScreenOnMin   = static_cast<quint8>(bytes[13]);
        item.nScreenOffHour = static_cast<quint8>(bytes[14]);
        item.nScreenOffMin  = static_cast<quint8>(bytes[15]);
    } catch (...) {
    }
    return item;
}

SSEnv2Sensor SSEnvManager::convertShockInfo(const QByteArray &bytes)
{
    SSEnv2Sensor item;
    try {
        if (bytes.size() != 4) return item;
        item.setPeriod       = static_cast<quint8>(bytes[0]);
        item.setShock        = static_cast<quint8>(bytes[1]);
        item.setIllumination = static_cast<quint8>(bytes[2]);
        item.setLuminance    = static_cast<quint8>(bytes[3]);
    } catch (...) {
    }
    return item;
}

SSEnv2BoardState SSEnvManager::convertBoardStateInfo(const QByteArray &bytes)
{
    SSEnv2BoardState item;
    try {
        if (bytes.size() != 30) return item;
        item.mActiveDateTime = QString::fromLatin1(bytes.mid(0, 14));
        item.nNone1          = static_cast<quint8>(bytes[14]);
        item.nNone2          = static_cast<quint8>(bytes[15]);
        item.mBoardDateTime  = QString::fromLatin1(bytes.mid(16, 14));
    } catch (...) {
    }
    return item;
}

