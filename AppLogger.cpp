#include "AppLogger.h"
#include <QMetaObject>
#include <cstdio>


static QtMessageHandler s_previousHandler = nullptr;

static void qtCustomMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Still print to terminal / systemd journal
    if (s_previousHandler) {
        s_previousHandler(type, context, msg);
    }

    QString level;
    QString color;
    switch (type) {
    case QtDebugMsg:
        level = QStringLiteral("DBG");
        color = QStringLiteral("#AAAAAA");
        break;
    case QtInfoMsg:
        level = QStringLiteral("INF");
        color = QStringLiteral("#4FC3F7");
        break;
    case QtWarningMsg:
        level = QStringLiteral("WRN");
        color = QStringLiteral("#FFB74D");
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = QStringLiteral("ERR");
        color = QStringLiteral("#E57373");
        break;
    }

    // Safely queue onto main UI thread (works even from QtConcurrent worker threads)
    QMetaObject::invokeMethod(AppLogger::instance(),
                              "appendLog",
                              Qt::QueuedConnection,
                              Q_ARG(QString, level),
                              Q_ARG(QString, msg),
                              Q_ARG(QString, color));
}