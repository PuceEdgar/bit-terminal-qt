#include "SSEnvModules.h"
#include "BitConvertUtils.h"
#include <QDebug>
#include <exception>

SSEnvModules::SSEnvModules(QObject *parent)
    : QObject(parent), m_checkTimer(new QTimer(this))
{
    m_checkTimer->setInterval(1000);
    connect(m_checkTimer, &QTimer::timeout, this, &SSEnvModules::onCheckSerialPort);
}

SSEnvModules::~SSEnvModules()
{
    doFinal();
}

bool SSEnvModules::isOpen() const
{
    try {
        return (m_sp != nullptr && m_sp->isOpen());
    } catch (...) {
        return false;
    }
}

bool SSEnvModules::initProc(const QString &portName,
                            int baudRate,
                            QSerialPort::DataBits dataBits,
                            QSerialPort::StopBits stopBits,
                            QSerialPort::Parity parity,
                            QSerialPort::FlowControl flowControl)
{
    try {
        if (m_sp) {
            doFinal();
        }

        auto *port = new QSerialPort(this);
        port->setPortName(portName);
        port->setBaudRate(baudRate);
        port->setDataBits(dataBits);
        port->setStopBits(stopBits);
        port->setParity(parity);
        port->setFlowControl(flowControl);

        connect(port, &QSerialPort::readyRead, this, &SSEnvModules::onReadyRead);
        connect(port, &QSerialPort::errorOccurred, this, &SSEnvModules::onErrorOccurred);

        if (!port->open(QIODevice::ReadWrite)) {
            qWarning() << "Failed to open serial port:" << portName << port->errorString();
            port->deleteLater();
            return false;
        }

        port->setDataTerminalReady(true);
        port->setRequestToSend(true);

        m_sp = port;
        m_receiveBuffer.clear();
        qInfo() << "Serial Port Opened:" << portName << "BaudRate:" << baudRate;

        if (!m_checkTimer->isActive()) {
            m_checkTimer->start();
        }
        return true;
    } catch (const std::exception &ex) {
        qCritical() << "initProc exception:" << ex.what();
        return false;
    } catch (...) {
        return false;
    }
}

void SSEnvModules::doFinal()
{
    try {
        if (m_checkTimer && m_checkTimer->isActive()) {
            m_checkTimer->stop();
        }
        if (m_sp) {
            if (m_sp->isOpen()) {
                m_sp->close();
            }
            m_sp->deleteLater();
            m_sp = nullptr;
        }
    } catch (const std::exception &ex) {
        qWarning() << "doFinal exception:" << ex.what();
    } catch (...) {
    }
}

bool SSEnvModules::sendData(const QByteArray &byteSendData)
{
    return sendData(byteSendData, 0, byteSendData.size());
}

bool SSEnvModules::sendData(const QByteArray &byteSendData, int offset, int count)
{
    try {
        if (m_sp != nullptr && m_sp->isOpen()) {
            QByteArray slice = byteSendData.mid(offset, count);
            qint64 written = m_sp->write(slice);

            if (written != slice.size()) {
                qWarning() << "[Tx] Partial write:" << written << "of" << slice.size() << m_sp->errorString();
            }

            // Ensure all 61KB+ bytes are actually pushed out to the UART driver
            while (m_sp->bytesToWrite() > 0) {
                if (!m_sp->waitForBytesWritten(3000)) {
                    qWarning() << "[Tx] waitForBytesWritten timeout, remaining:" << m_sp->bytesToWrite() << m_sp->errorString();
                    break;
                }
            }

            if (count > 30) {
                qDebug() << "[Tx]" << count << ":" << BitConvertUtils::byteArrayToHexStr(slice.left(30), QStringLiteral("-"));
            } else {
                qDebug() << "[Tx]" << count << ":" << BitConvertUtils::byteArrayToHexStr(slice, QStringLiteral("-"));
            }
            return written == slice.size();
        }
    } catch (const std::exception &ex) {
        qWarning() << "[Tx] Exception:" << ex.what();
    } catch (...) {
    }
    return false;
}


void SSEnvModules::onReadyRead()
{
    try {
        if (!m_sp) return;
        m_receiveBuffer.append(m_sp->readAll());

        while (true) {
            if (m_receiveBuffer.size() < 8)
                break;

            int stxIndex = -1;
            for (int i = 0; i < m_receiveBuffer.size() - 1; ++i) {
                if (static_cast<quint8>(m_receiveBuffer[i]) == 0x7E &&
                    static_cast<quint8>(m_receiveBuffer[i + 1]) == 0x7E) {
                    stxIndex = i;
                    break;
                }
            }

            if (stxIndex < 0) {
                int lastStx = m_receiveBuffer.lastIndexOf(char(0x7E));
                if (lastStx >= 0) {
                    m_receiveBuffer.remove(0, lastStx);
                } else {
                    m_receiveBuffer.clear();
                }
                break;
            }

            if (stxIndex > 0) {
                m_receiveBuffer.remove(0, stxIndex);
            }

            if (m_receiveBuffer.size() < 8)
                break;

            int size = BitConvertUtils::toInt16(m_receiveBuffer.mid(2, 2)) - 3;
            int totalPacketLength = 7 + size + 1;

            if (size < 0 || totalPacketLength > 65536) {
                m_receiveBuffer.remove(0, 2); // Discard bad header and resync
                continue;
            }

            if (m_receiveBuffer.size() < totalPacketLength)
                break;

            QByteArray packet = m_receiveBuffer.left(totalPacketLength);
            quint8 calcChecksum = BitConvertUtils::createCheckSum(packet, packet.size() - 1);
            quint8 packetChecksum = static_cast<quint8>(packet[packet.size() - 1]);

            if (packetChecksum != calcChecksum) {
                qWarning() << "[Rx] Checksum Error:" << BitConvertUtils::byteArrayToHexStr(packet, QStringLiteral("-"));
                m_receiveBuffer.remove(0, totalPacketLength);
                continue;
            }

            qDebug() << "[Rx]" << packet.size() << ":" << BitConvertUtils::byteArrayToHexStr(packet, QStringLiteral("-"));
            emit dataReceived(packet);

            m_receiveBuffer.remove(0, totalPacketLength);
        }
    } catch (const std::exception &ex) {
        qWarning() << "[Rx] Exception:" << ex.what();
    } catch (...) {
    }
}

void SSEnvModules::onErrorOccurred(QSerialPort::SerialPortError error)
{
    try {
        if (error == QSerialPort::NoError) return;
        qWarning() << "[Rx] Serial Error:" << error << (m_sp ? m_sp->errorString() : QString());
        emit serialPortError();
    } catch (...) {
    }
}

void SSEnvModules::onCheckSerialPort()
{
    try {
        if (m_sp == nullptr || !m_sp->isOpen()) {
            qDebug() << "SerialPort DisConnected";
            m_checkTimer->stop();
            emit disconnected();
        }
    } catch (...) {
    }
}
