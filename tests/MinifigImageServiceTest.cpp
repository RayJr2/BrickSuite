#include "../src/services/images/MinifigImageService.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {
QByteArray cacheKey(const QString& minifigNumber)
{
    const QByteArray identity = QByteArrayLiteral("Rebrickable\0")
                                + minifigNumber.trimmed().toUtf8();
    return QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex();
}

bool waitForImages(MinifigImageService& service,
                   const QList<QPair<QString, QUrl>>& requests,
                   QHash<QString, QString>& paths)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&service,
                     &MinifigImageService::imageReady,
                     &loop,
                     [&](const QString& number, const QString& path) {
                         paths.insert(number, path);
                         if (paths.size() == requests.size())
                             loop.quit();
                     });
    for (const auto& request : requests)
        service.requestMinifigImage(request.first, request.second.toString());
    timeout.start(5000);
    loop.exec();
    return paths.size() == requests.size();
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName("BrickSuiteTests");
    QCoreApplication::setApplicationName("MinifigImageServiceTest");
    QStandardPaths::setTestModeEnabled(true);

    const QString appData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir(appData).removeRecursively();

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0))
        return 1;

    QByteArray png;
    QBuffer pngBuffer(&png);
    pngBuffer.open(QIODevice::WriteOnly);
    QImage fixture(1, 1, QImage::Format_ARGB32);
    fixture.fill(Qt::red);
    if (!fixture.save(&pngBuffer, "PNG"))
        return 1;
    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        while (QTcpSocket* socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, [socket, png]() {
                socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: "
                              + QByteArray::number(png.size()) + "\r\nConnection: close\r\n\r\n"
                              + png);
                socket->disconnectFromHost();
            });
        }
    });

    MinifigImageService service;
    const QUrl base(QString("http://127.0.0.1:%1/image.png").arg(server.serverPort()));
    QHash<QString, QString> paths;
    if (!waitForImages(service, {{"fig/a", base}, {"fig_a", base}}, paths)
        || paths.value("fig/a") == paths.value("fig_a")
        || !QFile::exists(paths.value("fig/a")) || !QFile::exists(paths.value("fig_a"))) {
        QDir(appData).removeRecursively();
        return 2;
    }

    const QString corruptNumber = QStringLiteral("fig-corrupt");
    const QString corruptPath = QDir(appData).filePath(
        QString("cache/minifigs/%1.jpg").arg(QString::fromLatin1(cacheKey(corruptNumber))));
    QDir().mkpath(QFileInfo(corruptPath).absolutePath());
    QFile corruptFile(corruptPath);
    if (!corruptFile.open(QIODevice::WriteOnly)
        || corruptFile.write("not an image") < 0) {
        QDir(appData).removeRecursively();
        return 3;
    }
    corruptFile.close();
    if (!service.cachedImagePath(corruptNumber).isEmpty() || QFile::exists(corruptPath)) {
        QDir(appData).removeRecursively();
        return 4;
    }

    paths.clear();
    if (!waitForImages(service, {{corruptNumber, base}}, paths)
        || !QFile::exists(paths.value(corruptNumber))) {
        QDir(appData).removeRecursively();
        return 5;
    }

    QDir(appData).removeRecursively();
    return 0;
}
