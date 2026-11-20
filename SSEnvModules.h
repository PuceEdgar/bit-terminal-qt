#ifndef SSENVMODULES_H
#define SSENVMODULES_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>

class SSEnvModules : public QObject
{
    Q_OBJECT
public:
    explicit SSEnvModules(QObject *parent = nullptr);
    ~SSEnvModules() override;

    bool isOpen() const;
    bool initProc(const QString &portName,
                  int baudRate,
                  QSerialPort::DataBits dataBits = QSerialPort::Data8,
                  QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                  QSerialPort::Parity parity = QSerialPort::NoParity,
                  QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl);
    void doFinal();
    bool sendData(const QByteArray &byteSendData);
    bool sendData(const QByteArray &byteSendData, int offset, int count);

signals:
    void dataReceived(const QByteArray &receiveData);
    void disconnected();
    void serialPortError();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onCheckSerialPort();

private:
    QSerialPort *m_sp = nullptr;
    QTimer *m_checkTimer = nullptr;
    QByteArray m_receiveBuffer;
};

#endif // SSENVMODULES_H
