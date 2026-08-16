#pragma once

#include <QFile>
#include <QMutex>
#include <QString>
#include <QtGlobal>

class Logger
{
public:
    static bool init();
    static void shutdown();

    static QString logFilePath();
    static QString logDirectoryPath();

    static bool clear(QString* errorMessage = nullptr);

private:
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& message);

    static bool openForAppend();
    static bool rotateIfNeeded();

    static QFile s_logFile;
    static QMutex s_logMutex;
    static QString s_logFilePath;
    static QtMessageHandler s_previousHandler;
};
