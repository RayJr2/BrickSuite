/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../src/ui/common/SupportLinks.h"
#include "SupportNetworkFixture.h"
#include <QApplication>
#include <QMessageBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <cstdio>

namespace {
int checks = 0, failures = 0;
void check(bool value, const char* label) {
    ++checks; if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
void wait(int ms) {
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < ms) { QCoreApplication::processEvents(); QThread::msleep(1); }
}
bool until(const std::function<bool()>& ready, int timeout = 1000) {
    QElapsedTimer timer; timer.start();
    while (!ready() && timer.elapsed() < timeout) wait(1);
    return ready();
}
void selection(int status, QByteArray body, QNetworkReply::NetworkError error,
               bool primary, qint64 length = -1, bool redirect = false, int delay = 0) {
    SupportFixtureNetwork net; net.status = status; net.body = body; net.error = error;
    net.length = length; net.redirect = redirect; net.delay = delay;
    const int deadlineMs = delay < 0 ? 40 : 2500;
    SupportAvailabilityService selector(nullptr, &net, deadlineMs);
    SupportLinks::Coordinator controller(nullptr, &selector);
    QWidget owner, other; QList<QUrl> opened;
    const auto browser = [&](const QUrl& url) { opened << url; return true; };
    check(net.calls == 0, "construction never probes");
    controller.start(&owner, browser); controller.start(&other, browser);
    check(until([&] { return !opened.isEmpty(); }), "settles within bound");
    if (opened.size() != 1 || opened[0] != QUrl(primary ? AppConstants::SupportCheckoutUrl : AppConstants::SupportPayPalFallbackUrl))
        std::fprintf(stderr,"Selection status=%d body=%s error=%d length=%lld redirect=%d delay=%d\n",status,body.toHex().constData(),int(error),static_cast<long long>(length),redirect,delay);
    check(opened.size() == 1 && opened[0] == QUrl(primary ? AppConstants::SupportCheckoutUrl : AppConstants::SupportPayPalFallbackUrl), "exact URL selection");
    controller.start(&other, browser); wait(5);
    check(opened.size() == 1 && net.calls == 1, "coalesces pending and guarded action");
    check(!selector.busy(), "reply settled before handoff");
    check(net.request.url() == QUrl(AppConstants::SupportAvailabilityUrl)
        && net.request.url().userInfo().isEmpty() && !net.request.url().hasQuery(), "fixed anonymous endpoint");
    check(net.operation == QNetworkAccessManager::GetOperation && !net.sentBody, "one GET without body");
    for (auto attribute : {QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::AuthenticationReuseAttribute})
        check(net.request.attribute(attribute).toInt() == int(QNetworkRequest::Manual), "cookies/auth disabled");
    check(net.request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt() == int(QNetworkRequest::ManualRedirectPolicy), "manual redirect policy");
    check(net.request.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt() == int(QNetworkRequest::AlwaysNetwork)
        && !net.request.attribute(QNetworkRequest::CacheSaveControlAttribute).toBool(), "no stale or saved cache");
    check(net.request.rawHeader("Accept") == "text/plain" && net.request.rawHeader("Accept-Encoding") == "identity", "tiny plain response requested");
    check(!net.request.hasRawHeader("Authorization") && !net.request.hasRawHeader("Cookie"), "no credentials or cookies");
    check(net.request.transferTimeout() == deadlineMs, "transfer timeout set");
}
class LoopbackNetwork final : public QNetworkAccessManager {
public: QUrl local;
protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& req, QIODevice* body) override {
        auto request = req; request.setUrl(local);
        return QNetworkAccessManager::createRequest(op, request, body);
    }
};
void loopback(int mode) {
    QTcpServer server; check(server.listen(QHostAddress::LocalHost), "loopback listens");
    int hits = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, mode, &hits] {
            socket->readAll();
            if (socket->property("responded").toBool()) return;
            socket->setProperty("responded", true); ++hits;
            if (mode == 0) {
                socket->write("HTTP/1.1 302 Found\r\nLocation: /followed\r\nContent-Length: 0\r\n\r\n");
            } else if (mode == 1) {
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 1000000\r\n\r\n");
            } else if (mode == 2) {
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n" + QByteArray(65, 'x'));
            } else {
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
                auto* drip = new QTimer(socket);
                QObject::connect(drip, &QTimer::timeout, socket, [socket] { socket->write("r"); });
                drip->start(10); // Activity keeps arriving; the absolute deadline must still win.
            }
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    LoopbackNetwork net; net.local = QUrl(QString("http://127.0.0.1:%1/availability").arg(server.serverPort()));
    SupportAvailabilityService selector(nullptr, &net, 100);
    QList<QUrl> selected;
    QObject::connect(&selector, &SupportAvailabilityService::selected, &selector, [&](const QUrl& url) { selected << url; });
    QElapsedTimer elapsed; elapsed.start(); selector.start();
    check(until([&] { return !selected.isEmpty(); }), "real Qt response settles");
    check(selected.size() == 1 && selected[0] == QUrl(AppConstants::SupportPayPalFallbackUrl), "real Qt response falls back");
    check(elapsed.elapsed() < 500, "whole-operation bound despite slow stream");
    wait(20); if(hits != 1) std::fprintf(stderr,"Loopback mode=%d requests=%d\n",mode,hits);
    check(hits == 1 && selected.size() == 1, "no redirect follow or late selection");
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
    selection(200, "rfss-checkout-v1", QNetworkReply::NoError, true);
    for (const auto& body : {QByteArray(), QByteArray("wrong"), QByteArray("rfss-checkout-v1\n"),
         QByteArray(" rfss-checkout-v1"), QByteArray("rfss-checkout-v1 "), QByteArray("RFSS-checkout-v1"),
         QByteArray("rfss-checkout-v1suffix"), QByteArray("\xef\xbb\xbf") + "rfss-checkout-v1",
         QByteArray("rfss-checkout-v1\0", 17), QByteArray("rfss-check\0ut-v1", 15), QByteArray(65, 'x')})
        selection(200, body, QNetworkReply::NoError, false);
    for (int status : {204, 301, 302, 307, 308, 400, 404, 429, 500, 503})
        selection(status, "rfss-checkout-v1", QNetworkReply::NoError, false);
    for (auto error : {QNetworkReply::HostNotFoundError, QNetworkReply::SslHandshakeFailedError,
        QNetworkReply::ConnectionRefusedError, QNetworkReply::RemoteHostClosedError,
        QNetworkReply::TemporaryNetworkFailureError, QNetworkReply::NetworkSessionFailedError})
        selection(200, "rfss-checkout-v1", error, false);
    selection(200, "rfss-checkout-v1", QNetworkReply::NoError, false, 65);
    selection(200, "rfss-checkout-v1", QNetworkReply::NoError, false, -1, true);
    selection(200, {}, QNetworkReply::NoError, false, -1, false, -1);
    for (int mode = 0; mode < 6; ++mode) {
        SupportFixtureNetwork net; net.delay = 50;
        SupportAvailabilityService selector(nullptr, &net, 100);
        auto* controller = new SupportLinks::Coordinator(nullptr, &selector);
        auto* owner = new QWidget; owner->show(); int calls = 0;
        controller->start(owner, [&](const QUrl&) { ++calls; return true; });
        if (mode == 0) controller->cancel();
        if (mode == 1) owner->close();
        if (mode == 2) owner->hide();
        if (mode == 3) { delete owner; owner = nullptr; }
        if (mode == 4) QMetaObject::invokeMethod(&app, "aboutToQuit", Qt::DirectConnection);
        if (mode == 5) { delete controller; controller = nullptr; }
        wait(110); check(calls == 0 && !selector.busy(), "cancellation blocks late network handoff");
        delete controller; delete owner;
    }
    {
        SupportFixtureNetwork net; SupportAvailabilityService selector(nullptr, &net);
        SupportLinks::Coordinator controller(nullptr, &selector); QWidget owner; int calls = 0;
        SupportLinks::UrlOpener browser;
        browser = [&](const QUrl&) { ++calls; controller.start(&owner, browser); return true; };
        controller.start(&owner, browser); until([&] { return calls == 1; });
        check(calls == 1 && net.calls == 1, "browser reentrancy coalesced");
        wait(1020); controller.start(&owner, browser); until([&] { return calls == 2; });
        check(calls == 2 && net.calls == 2, "later deliberate action independent");
    }
    for (bool primary : {false, true}) {
        SupportFixtureNetwork net; if (!primary) net.status = 503;
        SupportAvailabilityService selector(nullptr, &net);
        SupportLinks::Coordinator controller(nullptr, &selector); QWidget owner;
        int opens = 0; bool warningSeen = false;
        QTimer dismiss; dismiss.setInterval(5);
        QObject::connect(&dismiss, &QTimer::timeout, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            warningSeen = box->text().contains(primary ? AppConstants::SupportCheckoutUrl : AppConstants::SupportPayPalFallbackUrl)
                && box->textFormat() == Qt::PlainText
                && box->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard);
            box->accept();
        });
        dismiss.start(); controller.start(&owner, [&](const QUrl&) { ++opens; return false; });
        until([&] { return warningSeen; }); wait(20);
        check(warningSeen && opens == 1 && net.calls == 1, "browser failure warns for selected URL without alternate launch");
    }
    for (int mode = 0; mode < 4; ++mode) loopback(mode);
    std::printf("Support checkout: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
