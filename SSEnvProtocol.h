#ifndef SSENVPROTOCOL_H
#define SSENVPROTOCOL_H
#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QtGlobal>

enum class SSEnv2MachineType : quint8 {
    Bit = 0x00,
    EnvBoard = 0x01
};

enum class SSEnv2EventType : quint8 {
    Default = 0x00,
    ShockEvent = 0x30,
    DoorEvent = 0x31
};

enum class SSEnv2OpcodeType : quint8 {
    Default = 0x00,
    Ack = 0x06,
    StatusRequest = 0x11,        // 상태정보요청
    DeviceControlSet = 0x12,     // 장치제어설정
    StatusResponse = 0x13,       // 상태정보응답
    VolumeRequest = 0x15,        // 볼륨상태요청
    VolumeSet = 0x16,            // 볼륨상태설정
    EnvBoardTimeRequest = 0x17,  // ENV보드시간요청 / 응답
    EnvBoardTimeSet = 0x18,      // ENV보드시간설정
    EnvBoardConfig = 0x19,       // ENV보드설정값
    EnvBoardConfigFan = 0x1A,    // ENV보드설정FAN
    EnvBoardConfigHeater = 0x1B, // ENV보드설정히터
    EnvBoardConfigLamp = 0x1C,   // ENV보드설정LAMP
    EnvBoardConfigScreen = 0x1D, // ENV보드설정화면장치
    EnvPeriodEtcSet = 0x24,      // ENV주기값등설정
    EnvPeriodRequest = 0x25,     // ENV주기정보요청
    KeypadInputEvent = 0x32,     // 키패드입력EVENT
    PcPowerOffEvent = 0x33,      // PC_PWR_OFF_EVENT
    TconBitmapWrite = 0x40,      // TCON_비트맵_쓰기
    TconBitmapWrite1 = 0x41,     // TCON_비트맵_쓰기1
    TconBitmapWrite2 = 0x42,     // TCON_비트맵_쓰기2
    TconBitmapWrite3 = 0x43,     // TCON_비트맵_쓰기3
    TconBitmapWrite4 = 0x44      // TCON_비트맵_쓰기4
};

struct SSEnv2Protocol {
    quint8 stx1 = 0;
    quint8 stx2 = 0;
    QByteArray size;
    quint8 machineCode = 0;
    SSEnv2MachineType mcGbn = SSEnv2MachineType::Bit;
    quint8 ctrlOpcode = 0;
    SSEnv2EventType eventGbn = SSEnv2EventType::Default;
    quint8 opcode = 0;
    SSEnv2OpcodeType opGbn = SSEnv2OpcodeType::Default;
    QByteArray data;
    quint8 checkSum = 0;

    int resultCd = -1;
    QString resultMsg;
};

struct SSEnv2Control {
    bool bAC5 = false;
    bool bDCADJ = false;
    bool bDC5V1 = false;
    bool bDC5V2 = false;
    bool bDC5V3 = false;
    bool bDC12V1 = false;
    bool bDC12V2 = false;
    bool bDC12V3 = false;
    bool bDC24V1 = false;
    bool bDC24V2 = false;
    bool bDCFAN = false;
    bool bAC1 = false;
    bool bAC2 = false;
    bool bAC3 = false;
    bool bAC4 = false;
    bool bDC12V4 = false;
};

struct SSEnv2State {
    qint16 nVersion = 0;
    float mainVoltage = 0.0f;
    float mainCurrent = 0.0f;
    float battVoltage = 0.0f;
    float battCurrent = 0.0f;
    float panlVoltage = 0.0f;
    float panlCurrent = 0.0f;
    qint8 temperature1 = 0;
    qint8 temperature2 = 0;
    int humidity = 0;
    int luminance = 0;
    int shock1 = 0;
    int door = 0;
    int illumination = 0;
    int battPercent = 0;
    qint16 remainBat = 0;

    int nMainPower = 0;
    int nBattPower = 0;
    int nNone1 = 0;
    int nNone2 = 0;
    quint8 baControl1 = 0;
    quint8 baControl2 = 0;
    SSEnv2Control controls;
    int nPeriod = 0;

    QString versionString() const {
        return QString("V %1.%2").arg(nVersion >> 8).arg(nVersion & 0xFF);
    }
    QString doorStateString() const {
        return door == 1 ? QStringLiteral("Open") : QStringLiteral("Close");
    }
};

struct SSEnv2Config {
    int nFanMode = 0;
    int nFanOnTemp = 0;
    int nFanOffTemp = 0;

    int nHeaterMode = 0;
    int nHeaterOnTemp = 0;
    int nHeaterOffTemp = 0;

    int nLampMode = 0;
    int nLampOnHour = 0;
    int nLampOnMin = 0;
    int nLampOffHour = 0;
    int nLampOffMin = 0;

    int nScreenMode = 0;
    int nScreenOnHour = 0;
    int nScreenOnMin = 0;
    int nScreenOffHour = 0;
    int nScreenOffMin = 0;
};

struct SSEnv2Sensor {
    int setPeriod = 0;
    int setShock = 0;
    int setLuminance = 0;
    int setIllumination = 0;
};

struct SSEnv2BoardState {
    QString mActiveDateTime;
    int nNone1 = 0;
    int nNone2 = 0;
    QString mBoardDateTime;

    static QString formatRawDateTime(const QString &ymd) {
        try {
            if (ymd.isEmpty() || ymd == QStringLiteral("00000000")) return QString();
            QDateTime dt;
            if (ymd.length() == 14) {
                dt = QDateTime::fromString(ymd, QStringLiteral("yyyyMMddHHmmss"));
            } else if (ymd.length() == 12) {
                dt = QDateTime::fromString(ymd, QStringLiteral("yyyyMMddHHmm"));
            } else if (ymd.length() == 8) {
                dt = QDateTime::fromString(ymd, QStringLiteral("yyyyMMdd"));
            } else {
                dt = QDateTime::fromString(ymd, Qt::ISODate);
            }
            return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
        } catch (...) {
            return QString();
        }
    }

    QString activeDateTimeFormatted() const { return formatRawDateTime(mActiveDateTime); }
    QString boardDateTimeFormatted() const { return formatRawDateTime(mBoardDateTime); }
};

#endif // SSENVPROTOCOL_H
