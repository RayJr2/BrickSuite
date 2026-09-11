#include "../src/database/DatabaseManager.h"
#include "../src/ui/builds/BuildActionEligibility.h"
#include "../src/ui/helpers/PartSearchCompleterHelper.h"

#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QLineEdit>
#include <QModelIndex>
#include <QPushButton>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <cstdio>

namespace {
bool require(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "%s\n", message);
    return value;
}

class Cleanup
{
public:
    explicit Cleanup(QString path)
        : m_path(std::move(path)) {}

    ~Cleanup()
    {
        DatabaseManager::instance().close();
        QDir(m_path).removeRecursively();
    }

private:
    QString m_path;
};
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("RFStateSideTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("PartSearchCompleter_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces));

    const QString path = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    Cleanup cleanup(path);

    if (!require(DatabaseManager::instance().initialize(), "Database initialization failed."))
        return 1;

    QSqlQuery query(DatabaseManager::instance().database());
    if (!require(query.exec(QStringLiteral(
            "INSERT INTO part_category(name,rebrickable_id,created_utc,modified_utc) "
            "VALUES('Test Bricks',990001,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)")),
            "Unable to create test category."))
        return 1;
    const int categoryId = query.lastInsertId().toInt();

    query.prepare(QStringLiteral(
        "INSERT INTO part(part_number,name,part_category_id,material,is_active,created_utc,modified_utc) "
        "VALUES(?,?,?,'Plastic',1,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)"));
    for (const auto& part : {
             qMakePair(QStringLiteral("30010"), QStringLiteral("Brick 2 x 4 Variant")),
             qMakePair(QStringLiteral("3001"), QStringLiteral("Brick 2 x 4")),
             qMakePair(QStringLiteral("brick-name"), QStringLiteral("3001 Named Match"))}) {
        query.bindValue(0, part.first);
        query.bindValue(1, part.second);
        query.bindValue(2, categoryId);
        if (!require(query.exec(), "Unable to create test Part."))
            return 1;
    }

    QLineEdit edit;
    QPushButton addRequirementButton;
    addRequirementButton.setEnabled(false);
    int resolutionChangeCount = 0;
    bool resolvableAtLastChange = false;
    PartSearchCompleterHelper::install(&edit, [&]() {
        ++resolutionChangeCount;
        resolvableAtLastChange = PartSearchCompleterHelper::hasResolvablePart(&edit);
        addRequirementButton.setEnabled(
            BuildActionEligibility::canSubmitRequirement(
                true, resolvableAtLastChange, true, true));
    });
    QTimer* searchTimer = edit.findChild<QTimer*>();
    if (!require(searchTimer != nullptr, "Part search debounce timer was not installed."))
        return 1;
    edit.setText(QStringLiteral("3001"));
    if (!require(addRequirementButton.isEnabled(),
                 "Exact manually typed Part did not enable Add Requirement."))
        return 1;
    QMetaObject::invokeMethod(searchTimer, "timeout", Qt::DirectConnection);

    QAbstractItemModel* model = edit.completer()->completionModel();
    if (!require(model->rowCount() == 3, "Expected three bounded matching suggestions."))
        return 1;
    edit.setProperty("canonicalPartNumber", QVariant());
    edit.setText(QStringLiteral("3001 — Brick 2 x 4"));
    if (!require(PartSearchCompleterHelper::canonicalPartNumber(&edit)
                     == QStringLiteral("3001")
                 && PartSearchCompleterHelper::hasResolvablePart(&edit)
                 && addRequirementButton.isEnabled(),
                 "Descriptive selected text did not resolve to canonical Part identity."))
        return 1;
    edit.clear();
    edit.setText(QStringLiteral("3001"));
    QMetaObject::invokeMethod(searchTimer, "timeout", Qt::DirectConnection);
    const QModelIndex exact = model->index(0, 0);
    if (!require(exact.data().toString() == QStringLiteral("3001 — Brick 2 x 4"),
                 "Exact Part number was not the first descriptive suggestion."))
        return 1;
    if (!require(exact.data(Qt::UserRole + 1).toString() == QStringLiteral("3001"),
                 "Suggestion does not retain its canonical Part number."))
        return 1;

    edit.setText(exact.data().toString());
    QMetaObject::invokeMethod(edit.completer(), "activated", Qt::DirectConnection,
                              Q_ARG(QModelIndex, exact));
    if (!require(edit.text() == QStringLiteral("3001 — Brick 2 x 4")
                 && PartSearchCompleterHelper::canonicalPartNumber(&edit)
                        == QStringLiteral("3001"),
                 "Suggestion activation did not preserve display text and canonical identity."))
        return 1;
    if (!require(PartSearchCompleterHelper::hasResolvablePart(&edit),
                 "Selected Part was not recognized as resolvable."))
        return 1;
    if (!require(resolutionChangeCount > 0 && resolvableAtLastChange,
                 "Selection callback ran before canonical identity became valid."))
        return 1;
    if (!require(addRequirementButton.isEnabled(),
                 "Selected suggestion did not enable Add Requirement."))
        return 1;

    const QModelIndex alternate = model->index(1, 0);
    edit.setText(alternate.data().toString());
    QMetaObject::invokeMethod(edit.completer(), "activated", Qt::DirectConnection,
                              Q_ARG(QModelIndex, alternate));
    if (!require(PartSearchCompleterHelper::canonicalPartNumber(&edit)
                    == QStringLiteral("30010"),
                 "Changing the selected suggestion did not change canonical identity."))
        return 1;

    edit.clear();
    if (!require(!resolvableAtLastChange,
                 "Programmatic clear did not invalidate the selected Part."))
        return 1;
    if (!require(!addRequirementButton.isEnabled(),
                 "Empty Part did not disable Add Requirement."))
        return 1;
    edit.setText(QStringLiteral("unknown-part"));
    QMetaObject::invokeMethod(searchTimer, "timeout", Qt::DirectConnection);
    if (!require(model->rowCount() == 0
                 && !PartSearchCompleterHelper::hasResolvablePart(&edit)
                 && !addRequirementButton.isEnabled(),
                 "Unknown Part unexpectedly resolved."))
        return 1;

    std::fprintf(stdout, "Part search completer validation passed.\n");
    return 0;
}
