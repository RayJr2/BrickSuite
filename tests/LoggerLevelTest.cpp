#include "../src/services/Logger.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <cstdio>

namespace { bool check(bool value,const char* message){if(!value)std::fprintf(stderr,"FAILED: %s\n",message);return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);
    app.setOrganizationName(QStringLiteral("RFStateSideTests"));
    app.setApplicationName(QStringLiteral("LoggerLevel"));
    QStandardPaths::setTestModeEnabled(true);
    qunsetenv("BRICKSUITE_DEBUG_LOGGING");
    bool ok=check(Logger::init(),"default logger initialization");
    Logger::clear();
    qDebug()<<"request queued forbidden-debug";
    qInfo()<<"retained-info";qWarning()<<"retained-warning";qCritical()<<"retained-critical";
    const QString path=Logger::logFilePath();Logger::shutdown();
    QFile file(path);ok&=check(file.open(QIODevice::ReadOnly|QIODevice::Text),"read default log");
    const QByteArray normal=file.readAll();file.close();
    ok&=check(!normal.contains("forbidden-debug"),"Debug excluded from default persistence");
    ok&=check(normal.contains("retained-info")&&normal.contains("retained-warning")
              &&normal.contains("retained-critical"),"Info Warning and Critical persist");

    qputenv("BRICKSUITE_DEBUG_LOGGING","1");
    ok&=check(Logger::init(),"diagnostic logger initialization");Logger::clear();
    qDebug()<<"diagnostic-debug";Logger::shutdown();
    ok&=check(file.open(QIODevice::ReadOnly|QIODevice::Text),"read diagnostic log");
    ok&=check(file.readAll().contains("diagnostic-debug"),"explicit diagnostic Debug persists");
    return ok?0:1;
}
