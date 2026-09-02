#include "../src/services/images/MinifigImageService.h"
#include "../src/services/images/SetImageService.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace {
int relevantWarnings = 0;
QtMessageHandler previousHandler = nullptr;

class TestCleanup
{
public:
    explicit TestCleanup(QString appData)
        : m_appData(std::move(appData))
    {
    }

    ~TestCleanup()
    {
        qInstallMessageHandler(previousHandler);
        QDir(m_appData).removeRecursively();
    }

private:
    QString m_appData;
};

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && message.contains("now known unavailable"))
        ++relevantWarnings;
    if (previousHandler)
        previousHandler(type, context, message);
}

template<typename Service, typename Signal, typename Request>
bool waitForSignal(Service& service, Signal signal, Request request)
{
    bool received = false;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&service, signal, &loop, [&](const QString&, const QString&) {
        received = true;
        loop.quit();
    });
    request();
    if (!received) {
        timeout.start(5000);
        loop.exec();
    }
    return received;
}

class TestServer : public QTcpServer
{
public:
    explicit TestServer(const QByteArray& png)
        : m_png(png)
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    if (socket->property("handled").toBool())
                        return;
                    const QByteArray request = socket->readAll();
                    const int firstSpace = request.indexOf(' ');
                    const int secondSpace = request.indexOf(' ', firstSpace + 1);
                    if (firstSpace < 0 || secondSpace < 0)
                        return;
                    socket->setProperty("handled", true);
                    const QString path = QString::fromUtf8(
                        request.mid(firstSpace + 1, secondSpace - firstSpace - 1));
                    const int count = ++m_counts[path];
                    if (path.contains("missing")) {
                        socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    } else if (path.contains("transient") && count == 1) {
                        socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    } else {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: "
                                      + QByteArray::number(m_png.size())
                                      + "\r\nConnection: close\r\n\r\n" + m_png);
                    }
                    socket->disconnectFromHost();
                });
            }
        });
    }

    QUrl url(const QString& path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(serverPort()).arg(path));
    }

    int count(const QString& path) const { return m_counts.value(QStringLiteral("/%1").arg(path)); }

private:
    QByteArray m_png;
    QHash<QString, int> m_counts;
};
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName("BrickSuiteTests");
    QCoreApplication::setApplicationName("CatalogImageNegativeCacheTest");
    QStandardPaths::setTestModeEnabled(true);
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(appData).removeRecursively();
    TestCleanup cleanup(appData);

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    QImage image(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    if (!image.save(&buffer, "PNG"))
        return 1;

    TestServer server(png);
    if (!server.listen(QHostAddress::LocalHost, 0))
        return 2;
    previousHandler = qInstallMessageHandler(messageHandler);

    const QString minifigMissing = server.url("minifig-missing").toString();
    {
        MinifigImageService service;
        if (!waitForSignal(service, &MinifigImageService::imageFailed, [&]() {
                service.requestMinifigImage("fig-test", minifigMissing);
            }) || !service.isImageKnownUnavailable("fig-test", minifigMissing))
            return 3;
    }
    if (server.count("minifig-missing") != 1 || relevantWarnings != 1)
        return 4;
    {
        MinifigImageService restartedService;
        if (!waitForSignal(restartedService, &MinifigImageService::imageFailed, [&]() {
                restartedService.requestMinifigImage("fig-test", minifigMissing);
            }))
            return 5;
    }
    if (server.count("minifig-missing") != 1 || relevantWarnings != 1)
        return 6;
    {
        MinifigImageService service;
        if (!waitForSignal(service, &MinifigImageService::imageReady, [&]() {
                service.requestMinifigImage("fig-test", server.url("minifig-new").toString());
            }))
            return 7;
        const QString transient = server.url("minifig-transient").toString();
        if (!waitForSignal(service, &MinifigImageService::imageFailed, [&]() {
                service.requestMinifigImage("fig-transient", transient);
            }) || service.isImageKnownUnavailable("fig-transient", transient)
            || !waitForSignal(service, &MinifigImageService::imageReady, [&]() {
                   service.requestMinifigImage("fig-transient", transient);
               }))
            return 8;
    }

    const QString setMissing = server.url("set-missing").toString();
    {
        SetImageService service;
        if (!waitForSignal(service, &SetImageService::imageFailed, [&]() {
                service.requestSetImage("404-1", setMissing);
            }) || !service.isImageKnownUnavailable("404-1", setMissing))
            return 9;
    }
    if (server.count("set-missing") != 1 || relevantWarnings != 2)
        return 10;
    {
        SetImageService restartedService;
        if (!waitForSignal(restartedService, &SetImageService::imageFailed, [&]() {
                restartedService.requestSetImage("404-1", setMissing);
            }))
            return 11;
    }
    if (server.count("set-missing") != 1 || relevantWarnings != 2)
        return 12;
    {
        SetImageService service;
        if (!waitForSignal(service, &SetImageService::imageReady, [&]() {
                service.requestSetImage("404-1", server.url("set-new").toString());
            }))
            return 13;
        const QString transient = server.url("set-transient").toString();
        if (!waitForSignal(service, &SetImageService::imageFailed, [&]() {
                service.requestSetImage("503-1", transient);
            }) || service.isImageKnownUnavailable("503-1", transient)
            || !waitForSignal(service, &SetImageService::imageReady, [&]() {
                   service.requestSetImage("503-1", transient);
               }))
            return 14;
    }

    if (server.count("minifig-new") != 1 || server.count("minifig-transient") != 2
        || server.count("set-new") != 1 || server.count("set-transient") != 2)
        return 15;

    return 0;
}
