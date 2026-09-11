#include "RemoteCollectionMutationDialog.h"
#include "RemoteCollectionStorageFilter.h"

#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"
#include "../../services/application/RemoteCollectionMutationApplicationService.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>

namespace {
void selectText(QComboBox* combo, const QString& value)
{
    const int index = combo->findText(value, Qt::MatchFixedString);
    if (index >= 0) combo->setCurrentIndex(index);
}
}

RemoteCollectionMutationDialog::RemoteCollectionMutationDialog(
    const QString& operation, int workspaceId,
    RemoteCollectionMutationApplicationService& service,
    const QList<RemoteReadDto::StorageSummary>& locations,
    const RemoteCollectionMutationDto::Request& seed,
    const QString& reference, const QString& title, QWidget* parent)
    : QDialog(parent), m_operation(operation), m_workspaceId(workspaceId),
      m_service(service), m_seed(seed)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(operation == QStringLiteral("collection.add")
        ? QStringLiteral("Add to Collection")
        : operation == QStringLiteral("collection.edit")
            ? QStringLiteral("Edit Collection Item")
            : seed.desiredActive ? QStringLiteral("Reactivate Collection Item")
                                 : QStringLiteral("Archive Collection Item"));
    resize(540, operation == QStringLiteral("collection.setActive") ? 220 : 390);
    HelpManager::setContextTopic(this, HelpTopic::MyCollection);
    auto* form = new QFormLayout(this);
    form->addRow(QStringLiteral("Reference:"), new QLabel(reference, this));
    auto* titleLabel = new QLabel(title, this); titleLabel->setWordWrap(true);
    form->addRow(QStringLiteral("Name:"), titleLabel);

    if (operation != QStringLiteral("collection.setActive")) {
        m_state = new QComboBox(this);
        m_state->addItem(QStringLiteral("Assembled"), QStringLiteral("Assembled"));
        m_state->addItem(QStringLiteral("Unassembled"), QStringLiteral("Unassembled"));
        m_state->addItem(QStringLiteral("Partially Assembled"), QStringLiteral("PartiallyAssembled"));
        m_state->addItem(QStringLiteral("Sealed"), QStringLiteral("Sealed"));
        m_condition = new QComboBox(this); m_condition->addItems({"Used", "New"});
        m_completeness = new QComboBox(this); m_completeness->addItems({"Unknown", "Complete", "Incomplete"});
        m_storage = new QComboBox(this); m_storage->addItem(QStringLiteral("Unassigned"), 0);
        for (const auto& location : RemoteCollectionStorageFilter::eligibleLeaves(locations))
            m_storage->addItem(location.displayPath, QVariant::fromValue<qint64>(location.storageId));
        m_nickname = new QLineEdit(seed.nickname, this);
        m_notes = new QTextEdit(seed.notes, this);
        const int stateIndex=m_state->findData(seed.state);if(stateIndex>=0)m_state->setCurrentIndex(stateIndex);
        selectText(m_condition, seed.condition);
        selectText(m_completeness, seed.completeness);
        const int storageIndex = m_storage->findData(seed.storageId);
        if (storageIndex >= 0) m_storage->setCurrentIndex(storageIndex);
        form->addRow(QStringLiteral("State:"), m_state);
        form->addRow(QStringLiteral("Condition:"), m_condition);
        form->addRow(QStringLiteral("Completeness:"), m_completeness);
        form->addRow(QStringLiteral("Collection Location:"), m_storage);
        form->addRow(QStringLiteral("Nickname:"), m_nickname);
        form->addRow(QStringLiteral("Notes:"), m_notes);
    } else {
        form->addRow(new QLabel(seed.desiredActive
            ? QStringLiteral("Reactivate this Collection item?")
            : QStringLiteral("Archive this Collection item? This does not delete it."), this));
    }
    m_status = new QLabel(this); m_status->setWordWrap(true); form->addRow(m_status);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(operation == QStringLiteral("collection.add")
        ? QStringLiteral("Add") : operation == QStringLiteral("collection.edit")
            ? QStringLiteral("Save") : seed.desiredActive ? QStringLiteral("Reactivate") : QStringLiteral("Archive"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &RemoteCollectionMutationDialog::submit);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons, &QDialogButtonBox::helpRequested, this, [this] { HelpManager::showTopic(HelpTopic::MyCollection, this); });
    form->addRow(m_buttons);
}

RemoteCollectionMutationDto::Request RemoteCollectionMutationDialog::request() const
{
    auto value = m_seed;
    value.workspaceId = m_workspaceId;
    value.mutationId = m_mutationId.isEmpty() ? RemoteMutationDto::newMutationId() : m_mutationId;
    if (m_state) {
        value.state = m_state->currentData().toString(); value.condition = m_condition->currentText();
        value.completeness = m_completeness->currentText(); value.storageId = m_storage->currentData().toLongLong();
        value.nickname = m_nickname->text().trimmed(); value.notes = m_notes->toPlainText().trimmed();
    }
    return value;
}

void RemoteCollectionMutationDialog::setPending(bool pending)
{
    m_pending = pending; m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!pending);
    const QList<QWidget*> fields{m_state, m_condition, m_completeness,
                                 m_storage, m_nickname, m_notes};
    for (QWidget* widget : fields) if (widget) widget->setEnabled(!pending);
}

void RemoteCollectionMutationDialog::submit()
{
    if (m_pending) return;
    const auto value = request(); m_mutationId = value.mutationId; setPending(true);
    m_status->setText(QStringLiteral("Saving to BrickSuite Host..."));
    m_service.submit(m_operation, value, this,
        [this](const RemoteCollectionMutationDto::Result& result) {
            setPending(false); m_mutationId.clear();
            emit mutationCompleted(result.item.value(QStringLiteral("collectionItemId")).toInt());
            accept();
        },
        [this](const RemoteMutationDto::Error& error) {
            if (error.outcome == RemoteMutationDto::Outcome::Unknown) {
                setPending(false);
                m_status->setText(QStringLiteral("The outcome is unknown. Retry Safely to check the same mutation."));
                m_buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Retry Safely"));
                return;
            }
            setPending(false); m_mutationId.clear(); m_status->setText(error.message);
            if (error.code == QStringLiteral("STALE_VERSION") || error.code == QStringLiteral("CONFLICT"))
                emit refreshRequired();
        });
}
