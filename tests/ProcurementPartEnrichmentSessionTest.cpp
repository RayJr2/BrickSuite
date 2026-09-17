#include "../src/services/procurement/ProcurementPartEnrichmentSession.h"

#include <QCoreApplication>
#include <QDebug>

namespace
{
bool require(bool condition, const QString& message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    ProcurementPartEnrichmentSession session;
    session.addEligibleRow(0, 10);
    session.addEligibleRow(1, 10);
    session.addEligibleRow(2, 20);
    session.addEligibleRow(2, 20);

    if (!require(session.eligiblePartIds() == QList<int>({10, 20}),
                 QStringLiteral("Eligible Part IDs were not deduplicated."))
        || !require(session.rowsForPart(10) == QList<int>({0, 1}),
                    QStringLiteral("Duplicate-Color source rows were not retained."))) {
        return 1;
    }

    session.markPending(session.eligiblePartIds());
    if (!require(session.totalPartCount() == 2 && session.pendingPartCount() == 2
                     && session.completedPartCount() == 0,
                 QStringLiteral("Initial progress state is incorrect.")))
        return 1;

    session.markComplete(10, false);
    session.markComplete(999, false);
    if (!require(session.completedPartCount() == 1 && session.pendingPartCount() == 1
                     && !session.contains(999),
                 QStringLiteral("Completion or unrelated-Part filtering is incorrect.")))
        return 1;

    session.markComplete(20, true);
    if (!require(session.completedPartCount() == 2 && session.pendingPartCount() == 0
                     && session.retryablePartIds() == QList<int>({20}),
                 QStringLiteral("Retryable completion state is incorrect.")))
        return 1;

    session.markPending(session.retryablePartIds());
    if (!require(session.isPending(20) && session.retryablePartIds().isEmpty(),
                 QStringLiteral("Explicit retry did not return the Part to pending.")))
        return 1;
    session.markComplete(20, false);

    ProcurementPartEnrichmentSession largeSession;
    for (int partId = 1; partId <= 25; ++partId)
        largeSession.addEligibleRow(partId - 1, partId);
    if (!require(largeSession.eligiblePartIds().size() == 25,
                 QStringLiteral("Procurement session truncated a request set larger than 20.")))
        return 1;

    ProcurementPartEnrichmentSession thunderWingsStyle;
    int row = 0;
    for (int partId = 1; partId <= 22; ++partId)
        thunderWingsStyle.addEligibleRow(row++, partId);
    for (int duplicatePartId : {5, 12, 19})
        thunderWingsStyle.addEligibleRow(row++, duplicatePartId);
    if (!require(row == 25 && thunderWingsStyle.eligiblePartIds().size() == 22,
                 QStringLiteral("Twenty-five source rows did not coalesce to 22 Part requests.")))
        return 1;

    qInfo() << "Procurement enrichment session validation passed.";
    return 0;
}
