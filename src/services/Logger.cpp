#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>

QFile Logger::s_logFile;
QMutex Logger::s_logMutex;
QString Logger::s_logFilePath;
QtMessageHandler Logger::s_previousHandler = nullptr;

namespace
{
constexpr qint64 MaximumLogSize = 2 * 1024 * 1024;
const QString CurrentLogFileName = "BrickSuite.log";
const QString PreviousLogFileName = "BrickSuite.log.1";
}

bool Logger::init()
{
    const QString dataPath
        = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    if (dataPath.isEmpty()) {
        std::fprintf(stderr, "Logger: AppLocalDataLocation is unavailable.\n");
        return false;
    }

    QDir directory(dataPath);

    if (!directory.exists() && !directory.mkpath(".")) {
        std::fprintf(stderr, "Logger: Unable to create application data directory.\n");
        return false;
    }

    s_logFilePath = directory.filePath(CurrentLogFileName);
    s_logFile.setFileName(s_logFilePath);

    if (!openForAppend()) {
        std::fprintf(stderr, "Logger: Unable to open BrickSuite.log.\n");
        return false;
    }

    s_previousHandler = qInstallMessageHandler(Logger::messageHandler);

    qInfo() << "BrickSuite application log started.";
    qInfo() << "Log file:" << s_logFilePath;

    return true;
}

void Logger::shutdown()
{
    qInfo() << "BrickSuite application log stopped.";

    qInstallMessageHandler(s_previousHandler);

    QMutexLocker locker(&s_logMutex);

    if (s_logFile.isOpen()) {
        s_logFile.flush();
        s_logFile.close();
    }
}

QString Logger::logFilePath()
{
    QMutexLocker locker(&s_logMutex);
    return s_logFilePath;
}

QString Logger::logDirectoryPath()
{
    const QString path = logFilePath();

    if (path.isEmpty())
        return QString();

    return QFileInfo(path).absolutePath();
}

bool Logger::clear(QString* errorMessage)
{
    QMutexLocker locker(&s_logMutex);

    if (s_logFilePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = "The BrickSuite log file has not been initialized.";

        return false;
    }

    if (s_logFile.isOpen()) {
        s_logFile.flush();
        s_logFile.close();
    }

    QFile file(s_logFilePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();

        s_logFile.setFileName(s_logFilePath);
        openForAppend();
        return false;
    }

    file.close();

    s_logFile.setFileName(s_logFilePath);

    if (!openForAppend()) {
        if (errorMessage)
            *errorMessage = "The log was cleared, but BrickSuite could not reopen it.";

        return false;
    }

    return true;
}

bool Logger::openForAppend()
{
    if (s_logFile.isOpen())
        return true;

    if (s_logFile.fileName().isEmpty())
        s_logFile.setFileName(s_logFilePath);

    return s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

bool Logger::rotateIfNeeded()
{
    if (!s_logFile.isOpen())
        return false;

    if (s_logFile.size() <= MaximumLogSize)
        return true;

    s_logFile.flush();
    s_logFile.close();

    const QFileInfo currentInfo(s_logFilePath);
    const QString previousPath
        = QDir(currentInfo.absolutePath()).filePath(PreviousLogFileName);

    if (QFile::exists(previousPath) && !QFile::remove(previousPath)) {
        std::fprintf(stderr, "Logger: Unable to remove previous rotated log.\n");
    }

    if (QFile::exists(s_logFilePath)
        && !QFile::rename(s_logFilePath, previousPath)) {
        std::fprintf(stderr, "Logger: Unable to rotate current log.\n");
    }

    s_logFile.setFileName(s_logFilePath);

    return openForAppend();
}

void Logger::messageHandler(QtMsgType type,
                            const QMessageLogContext& context,
                            const QString& message)
{
    Q_UNUSED(context);

    //
    // Keep known third-party image/platform noise out of the user-facing log.
    //
    if (message.contains("iCCP: known incorrect sRGB profile")
        || message.contains("ERROR:gbm_wrapper.cc")
        || message.contains("ERROR:v4l2_utils.cc")) {
        return;
    }

    QMutexLocker locker(&s_logMutex);

    if (!openForAppend())
        return;

    if (!rotateIfNeeded())
        return;

    QString level;

    switch (type) {
    case QtDebugMsg:
        level = "Debug";
        break;
    case QtInfoMsg:
        level = "Info";
        break;
    case QtWarningMsg:
        level = "Warning";
        break;
    case QtCriticalMsg:
        level = "Critical";
        break;
    case QtFatalMsg:
        level = "Fatal";
        break;
    }

    const QString timestamp
        = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    const QString formatted
        = QString("[%1] %2: %3").arg(timestamp, level, message);

    QTextStream stream(&s_logFile);
    stream << formatted << '\n';
    stream.flush();

#ifdef QT_DEBUG
    std::fprintf(stderr, "%s\n", qUtf8Printable(formatted));
    std::fflush(stderr);
#endif

    if (type == QtFatalMsg)
        std::abort();
}
