#include "../src/ui/builds/BuildActionEligibility.h"

#include <QCoreApplication>
#include <QDebug>

namespace {

bool check(bool condition, const char* description)
{
    if (!condition)
        qCritical() << "FAILED:" << description;

    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    bool ok = true;

    ok &= check(BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Planned")),
                "active planned Build from Stock is eligible");
    ok &= check(BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Pulling")),
                "active pulling Build from Stock is eligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Complete")),
                "completed Build from Stock is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Disassembled")),
                "disassembled Build from Stock is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("Stock"), QStringLiteral("Cancelled")),
                "cancelled Build from Stock is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("CompleteSet"), QStringLiteral("Planned")),
                "planned Complete Set is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    true, QStringLiteral("CompleteSet"), QStringLiteral("Complete")),
                "completed Complete Set is ineligible");
    ok &= check(!BuildActionEligibility::supportsStockFulfillment(
                    false, QStringLiteral("Stock"), QStringLiteral("Planned")),
                "inactive Build from Stock is ineligible");

    return ok ? 0 : 1;
}
