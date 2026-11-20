#include "BitConvertUtils.h"
#include <QtEndian>
#include <QStringList>
#include <QDebug>
#include <exception>

quint8 BitConvertUtils::createCheckSum(const QByteArray &bytes, int bLength)
{
    try {
        quint8 checkSum = 0;
        int limit = qMin(bLength, bytes.size());
        for (int index = 0; index < limit; ++index) {
            checkSum ^= static_cast<quint8>(bytes[index]);
        }
        return checkSum;
    } catch (const std::exception &ex) {
        qWarning() << "createCheckSum exception:" << ex.what();
        return 0;
    } catch (...) {
        return 0;
    }
}

qint16 BitConvertUtils::toInt16(const QByteArray &bArray, bool isBigEndianWire)
{
    try {
        if (bArray.size() < 2) return -1;
        const uchar *ptr = reinterpret_cast<const uchar *>(bArray.constData());
        return isBigEndianWire ? qFromBigEndian<qint16>(ptr) : qFromLittleEndian<qint16>(ptr);
    } catch (...) {
        return -1;
    }
}

QByteArray BitConvertUtils::fromInt16(qint16 nNo, bool isBigEndianWire)
{
    try {
        QByteArray out(2, '\0');
        uchar *ptr = reinterpret_cast<uchar *>(out.data());
        if (isBigEndianWire)
            qToBigEndian<qint16>(nNo, ptr);
        else
            qToLittleEndian<qint16>(nNo, ptr);
        return out;
    } catch (...) {
        return QByteArray();
    }
}

qint32 BitConvertUtils::toInt32(const QByteArray &bArray, bool isBigEndianWire)
{
    try {
        if (bArray.size() < 4) return -1;
        const uchar *ptr = reinterpret_cast<const uchar *>(bArray.constData());
        return isBigEndianWire ? qFromBigEndian<qint32>(ptr) : qFromLittleEndian<qint32>(ptr);
    } catch (...) {
        return -1;
    }
}

QByteArray BitConvertUtils::fromInt32(qint32 nNo, bool isBigEndianWire)
{
    try {
        QByteArray out(4, '\0');
        uchar *ptr = reinterpret_cast<uchar *>(out.data());
        if (isBigEndianWire)
            qToBigEndian<qint32>(nNo, ptr);
        else
            qToLittleEndian<qint32>(nNo, ptr);
        return out;
    } catch (...) {
        return QByteArray();
    }
}

QString BitConvertUtils::toStr(const QByteArray &bArray)
{
    try {
        QByteArray clean = bArray;
        clean.replace('\0', "");
        return QString::fromLatin1(clean);
    } catch (...) {
        return QString();
    }
}

QByteArray BitConvertUtils::fromStr(const QString &mStr, int fixByteLength, BitDummyType dummyGbn)
{
    try {
        char fillChar = (dummyGbn == BitDummyType::SpaceType) ? ' ' : '\0';
        QByteArray out(fixByteLength, fillChar);
        if (mStr.isNull()) return out;

        QByteArray encoded = mStr.toLatin1();
        int copyLen = qMin(encoded.size(), fixByteLength);
        for (int i = 0; i < copyLen; ++i) {
            out[i] = encoded[i];
        }
        return out;
    } catch (...) {
        return QByteArray();
    }
}

QString BitConvertUtils::byteArrayToHexStr(const QByteArray &bArray, const QString &splitStr)
{
    try {
        QStringList parts;
        parts.reserve(bArray.size());
        for (char c : bArray) {
            parts.append(QString("%1").arg(static_cast<quint8>(c), 2, 16, QLatin1Char('0')).toUpper());
        }
        return parts.join(splitStr);
    } catch (...) {
        return QString();
    }
}

QByteArray BitConvertUtils::hexStringToByteArray(const QString &logString, QChar splitCh)
{
    try {
        QByteArray out;
        const QStringList tokens = logString.split(splitCh, Qt::SkipEmptyParts);
        for (QString token : tokens) {
            token.remove(QStringLiteral("\r\n"));
            if (!token.isEmpty()) {
                bool ok = false;
                quint8 val = static_cast<quint8>(token.toUInt(&ok, 16));
                if (ok) out.append(static_cast<char>(val));
            }
        }
        return out;
    } catch (...) {
        return QByteArray();
    }
}

QByteArray BitConvertUtils::hexStringToByteArrayCompact(const QString &logString)
{
    try {
        QByteArray out;
        int pairs = logString.length() / 2;
        out.reserve(pairs);
        for (int i = 0; i < pairs; ++i) {
            bool ok = false;
            quint8 val = static_cast<quint8>(logString.mid(i * 2, 2).toUInt(&ok, 16));
            if (!ok) return QByteArray();
            out.append(static_cast<char>(val));
        }
        return out;
    } catch (...) {
        return QByteArray();
    }
}
