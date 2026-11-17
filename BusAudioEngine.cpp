#include "BusAudioEngine.h"

#include "BusAudioEngine.h"
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDebug>
#include <QtEndian>
#include <cstring>
#include <cmath>

static const QStringList kKoreanDigits = {
    QStringLiteral("공"), QStringLiteral("일"), QStringLiteral("이"),
    QStringLiteral("삼"), QStringLiteral("사"), QStringLiteral("오"),
    QStringLiteral("육"), QStringLiteral("칠"), QStringLiteral("팔"),
    QStringLiteral("구")
};

BusAudioEngine::BusAudioEngine(const QString &soundFolderPath, QObject *parent)
    : QObject(parent), m_soundFolderPath(soundFolderPath)
{
    m_targetFormat.setSampleRate(44100);
    m_targetFormat.setChannelCount(2);
    m_targetFormat.setSampleFormat(QAudioFormat::Int16);
}

BusAudioEngine::~BusAudioEngine()
{
    stop();
}

void BusAudioEngine::stop()
{
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    if (m_audioBuffer.isOpen()) {
        m_audioBuffer.close();
    }
}

void BusAudioEngine::playArriveSoonMessage(const QList<QPair<QString, QString>> &messageBuses)
{
    if (messageBuses.isEmpty()) return;

    stop();

    QStringList fileQueue;
    // for (const auto &bus : messageBuses) {
    //     announceComplexBus(bus.first, bus.second, fileQueue);
    // }
    qDebug() << "playing arrive soon...";
    for (int i = 0; i < messageBuses.size(); ++i) {
        announceComplexBus(messageBuses[i].first, messageBuses[i].second, fileQueue);
        if (i < messageBuses.size() - 1) {
            fileQueue << QStringLiteral("__PAUSE_120__");
        }
    }

    fileQueue << QStringLiteral("버스가.wav")
              << QStringLiteral("잠시_후.wav")
              << QStringLiteral("도착_합니다.wav");

    qDebug() << "added files to queue";
    m_pcmData.clear();
    // for (const QString &fileName : fileQueue) {
    //     appendWavFile(fileName, m_pcmData);
    // }

    for (const QString &fileName : fileQueue) {
        if (fileName == QStringLiteral("__PAUSE_120__")) {
            qDebug() << "append pause";
            appendPause(200, m_pcmData);
        } else {
            qDebug() << "append filename: " << fileName;
            appendWavFile(fileName, m_pcmData);
        }
    }

    if (m_pcmData.isEmpty()) {
        qWarning() << "BusAudioEngine: No valid PCM data generated.";
        return;
    }

    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (defaultDevice.isNull()) {
        qWarning() << "BusAudioEngine: No audio output device found.";
        return;
    }

    qDebug() << "prepare to speak...";
    m_audioSink = new QAudioSink(defaultDevice, m_targetFormat, this);
    m_audioBuffer.setData(m_pcmData);
    m_audioBuffer.open(QIODevice::ReadOnly);
    m_audioSink->start(&m_audioBuffer);
}

void BusAudioEngine::announceComplexBus(const QString &busLabel, const QString &busType, QStringList &fileQueue) const
{
    if (!busType.trimmed().isEmpty()) {
        fileQueue << QString("%1.wav").arg(busType)
        << QStringLiteral("버스.wav");
    }
    fileQueue.append(parseBusLabelToFiles(busLabel));
}

QStringList BusAudioEngine::parseBusLabelToFiles(const QString &busLabel) const
{
    QStringList files;
    if (busLabel.trimmed().isEmpty()) return files;

    // Match Korean words, English letters, continuous digits, or hyphens
    static const QRegularExpression regex(QStringLiteral("([가-힣]+)|([a-zA-Z]+)|(\\d+)|(-)"));
    QRegularExpressionMatchIterator it = regex.globalMatch(busLabel);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString value = match.captured(0);

        if (value[0].isDigit()) {
            if (value.startsWith('0')) {
                for (QChar ch : value) {
                    int digitVal = ch.digitValue();
                    if (digitVal >= 0 && digitVal <= 9) {
                        files << QString("%1.wav").arg(kKoreanDigits[digitVal]);
                    }
                }
            } else {
                files.append(getKoreanNumberFiles(value.toInt()));
            }
        } else if (value == QStringLiteral("-")) {
            files << QStringLiteral("다시.wav");
        } else {
            files << QString("%1.wav").arg(value.toLower());
        }
    }

    files << QStringLiteral("번.wav");
    return files;
}

QStringList BusAudioEngine::getKoreanNumberFiles(int number) const
{
    QStringList files;
    if (number <= 0) return files;

    int thousands = (number / 1000) % 10;
    int hundreds  = (number / 100) % 10;
    int tens      = (number / 10) % 10;
    int ones      = number % 10;

    if (thousands > 0) {
        if (thousands > 1) files << QString("%1.wav").arg(kKoreanDigits[thousands]);
        files << QStringLiteral("천.wav");
    }
    if (hundreds > 0) {
        if (hundreds > 1) files << QString("%1.wav").arg(kKoreanDigits[hundreds]);
        files << QStringLiteral("백.wav");
    }
    if (tens > 0) {
        if (tens > 1) files << QString("%1.wav").arg(kKoreanDigits[tens]);
        files << QStringLiteral("십.wav");
    }
    if (ones > 0) {
        files << QString("%1.wav").arg(kKoreanDigits[ones]);
    }

    return files;
}

void BusAudioEngine::appendPause(int milliseconds, QByteArray &outPcm) const
{
    int bytesPerSecond = m_targetFormat.sampleRate() * m_targetFormat.channelCount() * 2; // Int16 = 2 bytes
    int silentBytes = (bytesPerSecond * milliseconds) / 1000;
    outPcm.append(QByteArray(silentBytes, '\0'));
}

void BusAudioEngine::appendWavFile(const QString &fileName, QByteArray &outPcm) const
{
    QString fullPath = QDir(m_soundFolderPath).filePath(fileName);
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Missing audio asset ->" << fullPath;
        return;
    }

    QByteArray raw = file.readAll();
    if (raw.size() < 44 || std::memcmp(raw.constData(), "RIFF", 4) != 0) {
        qWarning() << "Invalid WAV file ->" << fullPath;
        return;
    }

    // Parse RIFF chunks to locate "fmt " and "data"
    quint16 audioFormat = 1; // 1 = PCM, 3 = IEEE Float
    quint16 numChannels = 1;
    quint32 sampleRate = 44100;
    quint16 bitsPerSample = 16;
    const char *dataPtr = nullptr;
    quint32 dataSize = 0;

    int pos = 12;
    while (pos + 8 <= raw.size()) {
        const char *chunkId = raw.constData() + pos;
        quint32 chunkSize = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(raw.constData() + pos + 4));
        if (pos + 8 + static_cast<int>(chunkSize) > raw.size()) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            const uchar *fmt = reinterpret_cast<const uchar *>(raw.constData() + pos + 8);
            audioFormat   = qFromLittleEndian<quint16>(fmt);
            numChannels   = qFromLittleEndian<quint16>(fmt + 2);
            sampleRate    = qFromLittleEndian<quint32>(fmt + 4);
            bitsPerSample = qFromLittleEndian<quint16>(fmt + 14);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataPtr  = raw.constData() + pos + 8;
            dataSize = chunkSize;
            break;
        }
        pos += 8 + chunkSize + (chunkSize % 2); // Word-aligned chunks
    }

    if (!dataPtr || dataSize == 0 || numChannels == 0) return;

    int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample <= 0 || numChannels == 0) {
        qWarning() << "Unsupported WAV format in ->" << fullPath << "bits:" << bitsPerSample;
        return;
    }

    // Decode input frames into normalized stereo float pairs
    int frameCount = dataSize / (numChannels * bytesPerSample);
    QList<QPair<float, float>> stereoFrames;
    stereoFrames.reserve(frameCount);

    for (int i = 0; i < frameCount; ++i) {
        const uchar *framePtr = reinterpret_cast<const uchar *>(dataPtr + i * numChannels * bytesPerSample);
        auto readSample = [&](int ch) -> float {
            const uchar *sPtr = framePtr + ch * bytesPerSample;
            if (audioFormat == 3 && bitsPerSample == 32) {
                float val;
                std::memcpy(&val, sPtr, sizeof(float));
                return std::clamp(val, -1.0f, 1.0f);
            } else if (bitsPerSample == 16) {
                qint16 val = qFromLittleEndian<qint16>(sPtr);
                return val / 32768.0f;
            } else if (bitsPerSample == 8) {
                return (static_cast<int>(*sPtr) - 128) / 128.0f;
            }
            return 0.0f;
        };

        float left = readSample(0);
        float right = (numChannels > 1) ? readSample(1) : left;
        stereoFrames.append({left, right});
    }

    // // Linear resampling to 44100 Hz and encoding as Int16 stereo
    // double ratio = static_cast<double>(sampleRate) / m_targetFormat.sampleRate();

    if (stereoFrames.isEmpty()) return;

    // 1. Trim leading and trailing silence (noise floor threshold ~ -36 dB)
    const float silenceThreshold = 0.015f;
    int firstSoundIdx = 0;
    while (firstSoundIdx < stereoFrames.size() &&
           std::abs(stereoFrames[firstSoundIdx].first) < silenceThreshold &&
           std::abs(stereoFrames[firstSoundIdx].second) < silenceThreshold) {
        ++firstSoundIdx;
    }

    int lastSoundIdx = stereoFrames.size() - 1;
    while (lastSoundIdx > firstSoundIdx &&
           std::abs(stereoFrames[lastSoundIdx].first) < silenceThreshold &&
           std::abs(stereoFrames[lastSoundIdx].second) < silenceThreshold) {
        --lastSoundIdx;
    }

    // If the entire file was silent, skip it
    if (firstSoundIdx >= lastSoundIdx) return;

    // Keep a tiny 12 ms cushion so hard consonants (e.g., '백', '번', '버스가') aren't clipped
    int padFrames = static_cast<int>(sampleRate * 0.012);
    int startIdx = std::max(0, firstSoundIdx - padFrames);
    int endIdx   = std::min(static_cast<int>(stereoFrames.size()) - 1, lastSoundIdx + padFrames);
    int trimmedCount = endIdx - startIdx + 1;

    // 2. Resample trimmed range to 44100 Hz (with optional speed multiplier for natural pacing)
    const double speedFactor = 1.0; // 1.0 = original speed, 1.12 = 12% brisker Korean TTS cadence
    double ratio = (static_cast<double>(sampleRate) / m_targetFormat.sampleRate()) * speedFactor;
    int targetFrames = static_cast<int>(trimmedCount / ratio);

    for (int i = 0; i < targetFrames; ++i) {
        double srcPos = startIdx + (i * ratio);
        int idx0 = std::min(static_cast<int>(srcPos), endIdx);
        int idx1 = std::min(idx0 + 1, endIdx);
        double frac = srcPos - static_cast<int>(srcPos);

        float l = stereoFrames[idx0].first  * (1.0 - frac) + stereoFrames[idx1].first  * frac;
        float r = stereoFrames[idx0].second * (1.0 - frac) + stereoFrames[idx1].second * frac;

        qint16 outL = static_cast<qint16>(std::clamp(l * 32767.0f, -32768.0f, 32767.0f));
        qint16 outR = static_cast<qint16>(std::clamp(r * 32767.0f, -32768.0f, 32767.0f));

        char sampleBytes[4];
        qToLittleEndian(outL, reinterpret_cast<uchar *>(sampleBytes));
        qToLittleEndian(outR, reinterpret_cast<uchar *>(sampleBytes + 2));
        outPcm.append(sampleBytes, 4);
    }
}
