#ifndef BUSAUDIOENGINE_H
#define BUSAUDIOENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QBuffer>
#include <QAudioSink>
#include <QAudioFormat>
#include <QPair>
#include <QList>

class BusAudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit BusAudioEngine(const QString &soundFolderPath, QObject *parent = nullptr);
    ~BusAudioEngine() override;

    //pairs of <busLable, busType>
    void playArriveSoonMessage(const QList<QPair<QString, QString>> &messageBuses);
    void stop();

private:
    QString m_soundFolderPath;
    QAudioFormat m_targetFormat;
    QAudioSink *m_audioSink = nullptr;
    QBuffer m_audioBuffer;
    QByteArray m_pcmData;

    void announceComplexBus(const QString &busLabel, const QString &busType, QStringList &fileQueue) const;
    QStringList parseBusLabelToFiles(const QString &busLabel) const;
    QStringList getKoreanNumberFiles(int number) const;

    //read wav header + pcm
    void appendWavFile(const QString &fileName, QByteArray &outPcm) const;
    void appendPause(int milliseconds, QByteArray &outPcm) const;
};

#endif // BUSAUDIOENGINE_H
