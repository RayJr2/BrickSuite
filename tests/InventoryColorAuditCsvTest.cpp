#include "../src/services/inventory/InventoryColorAuditCsv.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QDebug>

namespace {
bool require(bool condition, const QString& message)
{ if (!condition) qCritical().noquote() << message; return condition; }

bool writeFile(const QString& path, const QByteArray& data)
{ QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size(); }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc == 2) {
        InventoryColorAuditCsv report;
        QString error;
        if (!require(report.load(QString::fromLocal8Bit(argv[1]), &error), error)) return 1;
        const auto stats = report.statistics();
        qInfo() << "Read-only audit report validation passed; rows" << report.rowCount()
                << "COLOR_MISMATCH" << stats.total << "pending" << stats.pending;
        return 0;
    }
    QTemporaryDir temp;
    if (!require(temp.isValid(), QStringLiteral("Temporary directory failed."))) return 1;
    const QString path = temp.filePath(QStringLiteral("audit.csv"));
    const QByteArray source =
        "workspace,inventory_record_id,part_number,part_description,stored_color_name,audit_status,known_colors,confidence,unknown_evidence\r\n"
        "Workshop,1,3001,Brick 2 x 4,Pink,COLOR_MISMATCH,Blue [1],HIGH,\"comma, and \"\"quote\"\"\"\r\n"
        "Workshop,2,3002,Brick 2 x 3,Red,COLOR_MISMATCH,\"Blue [1]; Red [4]\",REVIEW,keep\r\n"
        "Workshop,3,3003,Brick 2 x 2,Lime,NO_KNOWN_COLOR_DATA,,REVIEW,other\r\n";
    if (!require(writeFile(path, source), QStringLiteral("Fixture write failed."))) return 1;

    InventoryColorAuditCsv csv; QString error;
    if (!require(csv.load(path, &error), error)) return 1;
    auto stats = csv.statistics();
    if (!require(stats.total == 2 && stats.pending == 2 && csv.firstPendingRow() == 0,
                 QStringLiteral("Initial scope/statistics/resume failed."))) return 1;
    if (!require(csv.value(0, QStringLiteral("unknown_evidence")) == QStringLiteral("comma, and \"quote\""),
                 QStringLiteral("Quoted/escaped CSV parsing failed."))) return 1;
    if (!require(csv.saveDisposition(0, QStringLiteral("FIXED"), QStringLiteral("physically checked"), &error), error)) return 1;
    if (!require(QFileInfo::exists(InventoryColorAuditCsv::originalBackupPath(path)),
                 QStringLiteral("Original safety copy was not created."))) return 1;
    QFile backup(InventoryColorAuditCsv::originalBackupPath(path));
    if (!require(backup.open(QIODevice::ReadOnly) && backup.readAll() == source,
                 QStringLiteral("Original safety copy did not preserve the source."))) return 1;

    InventoryColorAuditCsv reopened;
    if (!require(reopened.load(path, &error), error)) return 1;
    stats = reopened.statistics();
    if (!require(stats.fixed == 1 && stats.pending == 1 && reopened.firstPendingRow() == 1
                 && reopened.value(0, QStringLiteral("review_note")) == QStringLiteral("physically checked")
                 && reopened.value(0, QStringLiteral("unknown_evidence")) == QStringLiteral("comma, and \"quote\""),
                 QStringLiteral("Resume, disposition, or unknown-column preservation failed."))) return 1;
    if (!require(reopened.saveDisposition(1, QStringLiteral("SKIPPED"), QString(), &error), error)) return 1;
    stats = reopened.statistics();
    if (!require(stats.pending == 0 && stats.skipped == 1 && reopened.reviewRows(true) == QVector<int>{1},
                 QStringLiteral("Skipped-row revisit/statistics failed."))) return 1;

    InventoryColorAuditCsv externallyChanged;
    if (!require(externallyChanged.load(path, &error), error)) return 1;
    QFile append(path);
    if (!require(append.open(QIODevice::Append) && append.write("\r\n") == 2,
                 QStringLiteral("External-change fixture failed."))) return 1;
    append.close();
    if (!require(!externallyChanged.saveDisposition(0, QStringLiteral("VERIFIED_OK"), QString(), &error)
                 && error.contains(QStringLiteral("changed outside")),
                 QStringLiteral("External modification was not rejected."))) return 1;

    InventoryColorAuditCsv missing;
    const QString invalidPath = temp.filePath(QStringLiteral("invalid.csv"));
    writeFile(invalidPath, "inventory_record_id,part_number\n1,3001\n");
    if (!require(!missing.load(invalidPath, &error) && error.contains(QStringLiteral("missing required column")),
                 QStringLiteral("Missing required columns were accepted."))) return 1;
    qInfo() << "Inventory Color Audit CSV validation passed.";
    return 0;
}
