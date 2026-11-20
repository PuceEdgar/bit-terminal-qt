#ifndef SSENVMANAGER_H
#define SSENVMANAGER_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QByteArray>
#include "SSEnvProtocol.h"
#include "SSEnvModules.h"

class SSEnvManager : public QObject
{
    Q_OBJECT
public:
    explicit SSEnvManager(QObject *parent = nullptr);
    ~SSEnvManager() override;

    void connectProc(const QString &portName, int baudRate);
    void disconnectProc();
    bool connectYN() const;

    // TCON Bitmap Write & RLE Compression
    void tconImageWrite(const QByteArray &bStr, int x, int y, int w, int h);
    void tconImageWrite3(const QByteArray &bStr, int x, int y, int w, int h);
    void tconImageWrite4(const QByteArray &bStr, int x, int y, int w, int h);
    QByteArray rleEncode(const QByteArray &src, int pixelCount);
    QByteArray rleEncodeBW(const QByteArray &src, int pixelCount);

    // Commands (PC -> ENV Board)
    void sendStatusRequest();
    void sendDeviceControlRequest(quint8 bControl1, quint8 bControl2, int periodVal, int pcControl);
    void sendVolumeRequest();
    void sendVolumeSet(int volumeLevel);
    void sendEnvBoardTimeRequest();
    void sendEnvBoardTimeSet(const QString &dateTime14);
    void sendEnvBoardConfigRequest();
    void sendDcFanSet(int mode, int onTemp, int offTemp);
    void sendHeaterSet(int mode, int onTemp, int offTemp);
    void sendLampSet(int mode, int onHour, int onMin, int offHour, int offMin);
    void sendScreenDeviceSet(int mode, int onHour, int onMin, int offHour, int offMin);
    void sendPeriodAndShockSet(int periodVal, int shockVal, int luminanceVal, int illuminationVal);
    void sendPeriodShockLuminanceRequest();

    SSEnv2State envData() const { return m_envData; }
    quint8 wfMode = 1;

signals:
    void boardStateReceived(const SSEnv2BoardState &data);
    void stateReceived(const SSEnv2State &data);
    void configReceived(const SSEnv2Config &data);
    void sensorReceived(const SSEnv2Sensor &data);
    void disconnected();
    void reconnectRequested();
    void serialPortError();

private slots:
    void onTimeTick();           // 100 ms Tx queue tick
    void onStatusInfoTick();     // 30 sec status poll
    void onStatusResponseTick(); // 60 sec watchdog
    void onModuleDataReceived(const QByteArray &receiveData);
    void onModuleDisconnected();
    void onModuleSpError();

private:
    static constexpr quint8 STX = 0x7E;

    SSEnvModules *m_env2 = nullptr;
    QTimer *m_dtTime = nullptr;
    QTimer *m_dtStatusInfo = nullptr;
    QTimer *m_dtStatusResponse = nullptr;

    int m_processKillTimerSecondCount = 0;
    bool m_bResponseTconOk = true;
    int m_nNoResponseTime = 0;
    int m_nResponseRestartCnt = 0;
    int m_nSendCnt = 0;
    int m_nTotalDiscardQue = 0;
    int m_retryPowerOffCount = 0;

    QQueue<QByteArray> m_arrayTx;
    SSEnv2State m_envData;

    void tickTconImageTransfer();
    bool searchNewItem();
    void updateEnvData(const SSEnv2State &newState);

    SSEnv2Protocol parseEnv2Protocol(const QByteArray &bytes);
    SSEnv2State recvStatusInfo(const QByteArray &bytes);
    SSEnv2Config recvEnvBoardConfig(const QByteArray &bytes);
    SSEnv2Sensor convertShockInfo(const QByteArray &bytes);
    SSEnv2BoardState convertBoardStateInfo(const QByteArray &bytes);
};

#endif // SSENVMANAGER_H
