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

// Disposable packaging acceptance tool, never compiled into the product.
#include "src/network/BrickSuiteHostIdentity.h"
#include "src/network/BrickSuiteProtocol.h"
#include "src/services/CredentialStore.h"
#include "src/services/geometry/print/McutMeshBooleanService.h"
#include "src/services/geometry/print/McutWorkerProtocol.h"
#include "src/services/geometry/print/BoundedGeometryWorker.h"
#include "src/services/geometry/print/PrintMeshAnalysis.h"
#include "src/api/rebrickable/RebrickableService.h"
#include "src/api/brickset/BricksetService.h"
#include "src/database/DatabaseManager.h"
#include "src/ui/help/HelpManager.h"
#include <QApplication>
#include <QImageReader>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QProcess>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTcpServer>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>
#include <mach-o/dyld.h>
#include <cmath>

using namespace PrintGeometry;
namespace {
bool check(bool passed, const char* name)
{
    qInfo().noquote() << (passed ? "PASS" : "FAIL") << name;
    return passed;
}
PrintMesh box(double x0, double x1)
{
    PrintMesh m;
    m.vertices={{x0,0,0},{x1,0,0},{x1,2,0},{x0,2,0},{x0,0,2},{x1,0,2},{x1,2,2},{x0,2,2}};
    m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}};
    return m;
}
class Server : public QTcpServer {
public:
    BrickSuiteHostIdentity::Result identity;
    void incomingConnection(qintptr fd) override
    {
        auto* socket = new QSslSocket(this);
        socket->setSocketDescriptor(fd);
        auto config = QSslConfiguration::defaultConfiguration();
        config.setProtocol(QSsl::TlsV1_2OrLater);
        config.setLocalCertificate(identity.certificate);
        config.setPrivateKey(identity.privateKey);
        config.setPeerVerifyMode(QSslSocket::VerifyNone);
        socket->setSslConfiguration(config);
        socket->startServerEncryption();
    }
};
bool tls(const BrickSuiteHostIdentity::Result& identity, bool correctPin)
{
    Server server;
    server.identity = identity;
    if (!server.listen(QHostAddress::LocalHost, 0)) return false;
    QSslSocket client;
    auto config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2OrLater);
    config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    client.setSslConfiguration(config);
    QEventLoop loop;
    bool encrypted = false;
    bool rejected = false;
    QObject::connect(&client, &QSslSocket::sslErrors, &client, [&](const QList<QSslError>& errors) {
        bool permitted = correctPin && BrickSuiteHostIdentity::fingerprint(client.peerCertificate()) == identity.fingerprint;
        for (const auto& error : errors)
            permitted &= error.error() == QSslError::SelfSignedCertificate || error.error() == QSslError::HostNameMismatch;
        if (permitted) client.ignoreSslErrors(errors);
    });
    QObject::connect(&client, &QSslSocket::encrypted, &loop, [&] { encrypted = true; loop.quit(); });
    QObject::connect(&client, &QSslSocket::errorOccurred, &loop, [&] { rejected = true; loop.quit(); });
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    client.connectToHostEncrypted("127.0.0.1", server.serverPort());
    loop.exec();
    return correctPin ? encrypted : rejected && !encrypted;
}
bool workers()
{
    const auto a = box(0,2), b = box(1,3);
    McutMeshBooleanService service(McutMeshBooleanService::Execution::Isolated);
    const auto result = service.unite(a,b);
    bool ok = check(result.ok() && validateBooleanOperand(result.resultAnalysis).ok()
                    && std::abs(result.resultAnalysis.absoluteVolume-12)<0.01, "dedicated bundled worker: valid 12 mm3 union");
    const auto difference = service.subtract(a,b);
    ok &= check(difference.ok() && std::abs(difference.resultAnalysis.absoluteVolume-4)<0.01, "dedicated bundled worker: 4 mm3 difference");
    const QString helper = QCoreApplication::applicationDirPath()+"/BrickSuiteMeshBooleanWorker";
    ok &= check(McutMeshBooleanService(helper, 3000, 1).unite(a,b).error == MeshBooleanError::ResourceLimitExceeded,
                "bundled worker memory watchdog rejects before admission at one-byte test budget");
    ok &= check(McutMeshBooleanService(helper, 0).unite(a,b).error == MeshBooleanError::ResourceLimitExceeded,
                "bundled worker deadline terminates at zero-ms test deadline");
    QTemporaryDir temp;
    QFile input(temp.filePath("input.bin"));
    if (!input.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&input); out.setVersion(QDataStream::Qt_6_0);
    out << quint32(1);
    McutWorkerProtocol::writeMesh(out,a); McutWorkerProtocol::writeMesh(out,b);
    input.close();
    QProcess process;
    process.setProgram(QCoreApplication::applicationDirPath()+"/BrickSuite");
    process.setArguments({"--local-override-union-worker", temp.path()});
    const auto outcome = BoundedGeometryWorker::run(process,10000);
    QFile output(temp.filePath("output.bin"));
    bool good = outcome == BoundedGeometryWorker::Outcome::Finished && process.exitCode()==0 && output.open(QIODevice::ReadOnly);
    if (good) {
        QDataStream in(&output); in.setVersion(QDataStream::Qt_6_0);
        quint32 version=0; bool accepted=false; QString diagnostic; PrintMesh mesh;
        in >> version >> accepted >> diagnostic;
        good = version==1 && accepted && McutWorkerProtocol::readMesh(in,mesh,10000) && in.atEnd();
        const auto analysis=analyzeSource(mesh);
        good &= validatePreparedMesh(analysis).ok() && std::abs(analysis.absoluteVolume-12)<0.01;
    }
    return check(good,"actual packaged BrickSuite Local Override self-launch and validated union") && ok;
}
bool credentialsAndApis(bool allowWrite = true, const QString& onlyName = {})
{
    bool ok=true;
    for (const QString name : {QStringLiteral("RebrickableApiKey"),QStringLiteral("BricksetApiKey"),QStringLiteral("BrickSuiteHostTlsIdentity")}) {
        if (!onlyName.isEmpty() && name != onlyName) continue;
        const auto read=CredentialStore::read(name);
        qInfo().noquote()<<"KEYCHAIN"<<name<<"read="<<read.success<<"found="<<read.found;
        if (!read.success || !read.found) { ok=false; continue; }
        // Re-save exactly the existing value; never create/rotate/delete production credentials.
        if (allowWrite) {
        QString error;
        const bool wrote=CredentialStore::write(name,read.value,&error);
        const auto again=CredentialStore::read(name);
        ok &= check(wrote && again.success && again.value==read.value,"existing credential round-trip unchanged");
        }
        if (name=="BrickSuiteHostTlsIdentity") {
            const auto identity=BrickSuiteHostIdentity::loadOrCreate();
            ok &= check(identity.success && identity.privateKey.length()==256,"existing Keychain P-256 Host identity loads");
            if (identity.success) ok &= check(tls(identity,true) && tls(identity,false),"existing identity TLS: matching pin connects, wrong pin rejects");
            continue;
        }
        QEventLoop loop; QTimer timer; timer.setSingleShot(true);
        QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);
        bool passed=false;
        RebrickableService rebrickable; BricksetService brickset;
        QObject::connect(&rebrickable,&RebrickableService::connectionTestFinished,&loop,[&](const auto& result){passed=result.success;qInfo()<<"Rebrickable HTTP"<<result.httpStatusCode;loop.quit();});
        QObject::connect(&brickset,&BricksetService::connectionTestFinished,&loop,[&](const auto& result){passed=result.success;qInfo()<<"Brickset HTTP"<<result.httpStatusCode;loop.quit();});
        timer.start(30000);
        if(name=="RebrickableApiKey") rebrickable.testConnection(read.value); else brickset.testConnection(read.value);
        loop.exec(); ok &= check(passed,"provider connection via production service");
    }
    return ok;
}
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    app.setOrganizationName("RFStateSidePackageAcceptance");
    app.setApplicationName("BrickSuiteM392");
    QStandardPaths::setTestModeEnabled(true);
    bool protocolOk=check(BrickSuiteProtocol::Major==1 && BrickSuiteProtocol::Minor==5, "Protocol 1.5");
    bool ok=check(QSslSocket::activeBackend()=="openssl", "OpenSSL selected without environment override");
    ok &= protocolOk;
    qInfo()<<"TLS VERSION"<<QSslSocket::sslLibraryVersionString();
    ok &= check(QSslSocket::sslLibraryVersionString().startsWith("OpenSSL 3.6.4 "), "expected OpenSSL 3.6.4 runtime");
    const auto identity=BrickSuiteHostIdentity::generateEphemeral();
    ok &= check(identity.success && identity.privateKey.algorithm()==QSsl::Ec && identity.privateKey.length()==256
                && BrickSuiteHostIdentity::validate(identity.certificate,identity.privateKey).success
                && tls(identity,true) && tls(identity,false),"ephemeral P-256 TLS and fingerprint rejection");
    RebrickableService rebrickable;
    BricksetService brickset;
    ok &= check(rebrickable.metaObject() && brickset.metaObject(), "provider services construct without credentials");
    ok &= workers();
    auto& db=DatabaseManager::instance();
    ok &= check(db.initialize(),"isolated SQLite database initializes");
    QSqlQuery query(db.database());
    ok &= check(query.exec("SELECT version FROM schema_version LIMIT 1") && query.next() && query.value(0).toInt()==35,"Schema 35");
    QTemporaryDir backupDirectory(QDir::tempPath()+"/bricksuite-m392-backup-XXXXXX");
    const auto backup=DatabaseManager::createVerifiedBackup(db.databasePath(), backupDirectory.filePath("BrickSuite.db"));
    ok &= check(backup.success && QFile::exists(backup.backupPath),"isolated database backup created and verified");
    db.close();
    for(const auto path : {":/help/index.html",":/help/printing.html",":/icons/bricksuite.ico"})
        ok &= check(QFile::exists(QString::fromLatin1(path)),path);
    ok &= check(HelpManager::shortcuts().contains(QKeySequence(Qt::Key_F1)), "F1 Help mapping");
    QImageReader icon(QStringLiteral(":/icons/bricksuite.ico"));
    ok &= check(!icon.read().isNull(), "embedded application icon decodes");
    if(app.arguments().contains("--credentials")) ok &= credentialsAndApis();
    if(app.arguments().contains("--brickset-read-only")) ok &= credentialsAndApis(false, QStringLiteral("BricksetApiKey"));
    const QString bundle=QDir(QCoreApplication::applicationDirPath()+"/../..").canonicalPath()+"/";
    bool closed=true; int bundled=0;
    for(uint32_t i=0;i<_dyld_image_count();++i) {
        const QString path=QString::fromLocal8Bit(_dyld_get_image_name(i));
        if(path.startsWith(bundle)) ++bundled;
        else if(!path.startsWith("/usr/lib/") && !path.startsWith("/System/Library/")) { closed=false; qWarning()<<"Non-bundle loaded image"<<path; }
    }
    qInfo()<<"Loaded bundle Mach-O images"<<bundled;
    ok &= check(closed,"runtime image closure: only bundle and Apple system libraries");
    return ok?0:1;
}
