#ifndef APPLOGGER_H
#define APPLOGGER_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>


struct LogEntry {
    QString timestamp;
    QString level;
    QString message;
    QString color;
};

class AppLogger : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        TimeStampRole = Qt::UserRole + 1,
        LevelRole,
        MessageRole,
        ColorRole
    };

    static AppLogger *instance();
    static void installMessageHandler();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray > roleNames() const override;

    Q_INVOKABLE void clear();

public slots:
    void appendLog(const QString &level, const QString &message, const QString &color);

private:
    explicit AppLogger(QObject *parent = nullptr);
    QList<LogEntry> m_logs;
    static constexpr int MaxLogEntries = 100;
};

#endif // APPLOGGER_H
