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
#include <QSettings>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cmath>
#include <QThread>
#include <QUuid>

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
bool containment()
{
    bool ok=true;
    for (const QString name : {QString("BrickSuiteMeshBooleanWorker"),QString("BrickSuite")}) {
        QTemporaryDir dir;
        const auto fifo=QFile::encodeName(dir.filePath("input.bin"));
        if (::mkfifo(fifo.constData(),0600)!=0) return false;
        QProcess child;
        child.setProgram(QCoreApplication::applicationDirPath()+"/"+name);
        child.setArguments(name=="BrickSuite" ? QStringList{"--local-override-union-worker",dir.path()} : QStringList{dir.path()});
        child.start();
        if (!child.waitForStarted()) return false;
        bool bounded=false;
        for (int i=0;i<100 && !bounded;++i) {
            QFile limits(QString("/proc/%1/limits").arg(child.processId()));
            if (limits.open(QIODevice::ReadOnly))
                bounded=QString::fromUtf8(limits.readAll()).contains(QRegularExpression("Max address space\\s+536870912\\s+"));
            if (!bounded) QThread::msleep(20);
        }
        child.kill();child.waitForFinished(3000);
        ok &= check(bounded,qPrintable(name+" actual child has 512 MiB RLIMIT_AS before reading operands"));
    }
    QProcess allocation;
    allocation.setProgram(QCoreApplication::applicationFilePath());
    allocation.setArguments({"--memory-limit-probe"});
    const auto outcome=BoundedGeometryWorker::run(allocation,10000);
    ok &= check(outcome==BoundedGeometryWorker::Outcome::Finished && allocation.exitCode()==0,
                "production child policy rejects a 600 MiB mapping");
    return ok;
}
QString hash(const QString& s) { return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(),QCryptographicHash::Sha256).toHex()); }
int credentials(const QString& mode)
{
    const QString output=qEnvironmentVariable("M39_AUDIT_OUTPUT");
    QSettings manifest(output+"/credential-hashes.ini",QSettings::IniFormat);
    const QStringList names{"RebrickableApiKey","BricksetApiKey","BrickSuiteClientPairedCredential","BrickSuitePairedDevice/m39-package"};
    QString error;
    if(mode=="cred-unavailable") {
        const auto r=CredentialStore::read("M39SyntheticUnavailable");
        return check(!r.success && !r.found && !r.error.isEmpty(),"unavailable Secret Service returns explicit backend failure")?0:1;
    }
    if(mode=="cred-write") {
        for(const auto& name:names) {
            const auto value=QUuid::createUuid().toString()+QUuid::createUuid().toString();
            if(!CredentialStore::write(name,value,&error))return 10;
            manifest.setValue(name,hash(value));
        }
        const auto id=BrickSuiteHostIdentity::loadOrCreate();if(!id.success)return 11;
        manifest.setValue("identity",id.fingerprint);manifest.sync();
    } else if(mode=="cred-read") {
        for(const auto& name:names) {
            const auto r=CredentialStore::read(name);
            if(!r.success||!r.found||hash(r.value)!=manifest.value(name).toString())return 12;
        }
        const auto id=BrickSuiteHostIdentity::loadOrCreate();
        if(!id.success||id.fingerprint!=manifest.value("identity").toString())return 13;
    } else if(mode=="cred-clean") {
        for(const auto& name:names+QStringList{"BrickSuiteHostTlsIdentity"})if(!CredentialStore::remove(name,&error))return 14;
    } else return 15;
    qInfo()<<"Synthetic credential phase passed"<<mode;return 0;
}
}
int main(int argc,char** argv)
{
    if(argc>1 && QByteArray(argv[1])=="--memory-limit-probe") {
        if(!BoundedGeometryWorker::constrainChild())return 20;
        void* p=::mmap(nullptr,600ULL*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(p!=MAP_FAILED){::munmap(p,600ULL*1024*1024);return 21;}
        return 0;
    }
    QApplication app(argc,argv);
    app.setOrganizationName("RFStateSidePackageAcceptance");
    app.setApplicationName("BrickSuiteM39L2");
    QStandardPaths::setTestModeEnabled(true);
    if (argc>1 && QString::fromLocal8Bit(argv[1]).startsWith("cred-")) return credentials(QString::fromLocal8Bit(argv[1]));
    bool protocolOk=check(BrickSuiteProtocol::Major==1 && BrickSuiteProtocol::Minor==5, "Protocol 1.5");
    bool ok=check(QSslSocket::activeBackend()=="openssl", "OpenSSL selected without environment override");
    ok &= protocolOk;
    qInfo()<<"TLS VERSION"<<QSslSocket::sslLibraryVersionString();
    ok &= check(QSslSocket::sslLibraryVersionString().startsWith("OpenSSL 3.5.8 "), "expected OpenSSL 3.5.8 runtime");
    const auto identity=BrickSuiteHostIdentity::generateEphemeral();
    ok &= check(identity.success && identity.privateKey.algorithm()==QSsl::Ec && identity.privateKey.length()==256
                && BrickSuiteHostIdentity::validate(identity.certificate,identity.privateKey).success
                && tls(identity,true) && tls(identity,false),"ephemeral P-256 TLS and fingerprint rejection");
    RebrickableService rebrickable;
    BricksetService brickset;
    ok &= check(rebrickable.metaObject() && brickset.metaObject(), "provider services construct without credentials");
    ok &= workers();
    ok &= containment();
    auto& db=DatabaseManager::instance();
    ok &= check(db.initialize(),"isolated SQLite database initializes");
    QSqlQuery query(db.database());
    ok &= check(query.exec("SELECT version FROM schema_version LIMIT 1") && query.next() && query.value(0).toInt()==35,"Schema 35");
    QTemporaryDir backupDirectory(QDir::tempPath()+"/bricksuite-m39l2-backup-XXXXXX");
    const auto backup=DatabaseManager::createVerifiedBackup(db.databasePath(), backupDirectory.filePath("BrickSuite.db"));
    ok &= check(backup.success && QFile::exists(backup.backupPath),"isolated database backup created and verified");
    db.close();
    for(const auto path : {":/help/index.html",":/help/printing.html",":/icons/bricksuite.ico"})
        ok &= check(QFile::exists(QString::fromLatin1(path)),path);
    ok &= check(HelpManager::shortcuts().contains(QKeySequence(Qt::Key_F1)), "F1 Help mapping");
    QImageReader icon(QStringLiteral(":/icons/bricksuite.ico"));
    ok &= check(!icon.read().isNull(), "embedded application icon decodes");
    const QString bundle=QDir(QCoreApplication::applicationDirPath()+"/..").canonicalPath()+"/";
    QFile maps("/proc/self/maps");bool closed=maps.open(QIODevice::ReadOnly);QSet<QString> images;
    if(closed) for(const auto& line:maps.readAll().split('\n')) {
        int slash=line.indexOf('/');if(slash<0)continue;
        const QString path=QString::fromLocal8Bit(line.mid(slash));
        if(!path.contains(".so") && !path.startsWith(bundle))continue;
        images.insert(path);
        if(!path.startsWith(bundle) && !path.startsWith("/lib/") && !path.startsWith("/lib64/") && !path.startsWith("/usr/lib/"))closed=false;
        if(path.contains("libQt6")||path.contains("libssl.so")||path.contains("libcrypto.so")||path.contains("libmcut"))closed &= path.startsWith(bundle);
    }
    QFile manifest(qEnvironmentVariable("M39_AUDIT_OUTPUT")+"/loaded-images.txt");
    if(manifest.open(QIODevice::WriteOnly))for(const auto& image:images)manifest.write(image.toUtf8()+"\n");
    ok &= check(closed,"loaded ELF closure uses only bundle and host system libraries");
    return ok?0:1;
}
