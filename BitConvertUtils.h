#ifndef BITCONVERTUTILS_H
#define BITCONVERTUTILS_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

enum class BitDummyType : quint8 {
    NullType = 0,
    SpaceType = 32 // 0x20
};

class BitConvertUtils
{
public:
    static quint8 createCheckSum(const QByteArray &bytes, int bLength);
    static qint16 toInt16(const QByteArray &bArray, bool isBigEndianWire = true);
    static QByteArray fromInt16(qint16 nNo, bool isBigEndianWire = true);
    static qint32 toInt32(const QByteArray &bArray, bool isBigEndianWire = true);
    static QByteArray fromInt32(qint32 nNo, bool isBigEndianWire = true);

    static QString toStr(const QByteArray &bArray);
    static QByteArray fromStr(const QString &mStr, int fixByteLength, BitDummyType dummyGbn = BitDummyType::NullType);

    static QString byteArrayToHexStr(const QByteArray &bArray, const QString &splitStr = QStringLiteral(" "));
    static QByteArray hexStringToByteArray(const QString &logString, QChar splitCh = QLatin1Char(' '));
    static QByteArray hexStringToByteArrayCompact(const QString &logString);
};

#endif // BITCONVERTUTILS_H
