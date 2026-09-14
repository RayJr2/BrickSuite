#include "InventoryColorAuditReviewDialog.h"

#include "EditInventoryDialog.h"
#include "../../app/WorkspaceContext.h"
#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"
#include "../../models/Workspace.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

InventoryColorAuditReviewDialog::InventoryColorAuditReviewDialog(
    const QString& fileName, WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent), m_workspaceContext(workspaceContext)
{
    setWindowTitle(QStringLiteral("Inventory Color Audit Review"));
    setWindowModality(Qt::NonModal);
    resize(820, 650);

    QString error;
    if (!m_csv.load(fileName, &error)) {
        QMessageBox::critical(parent, windowTitle(), error);
        return;
    }
    m_ready = true;
    m_reviewRows = m_csv.reviewRows();

    auto* root = new QVBoxLayout(this);
    auto* explanation = new QLabel(
        QStringLiteral("Physically inspect each part. Suggestions are audit evidence only; "
                       "all Inventory changes use the normal Edit Inventory workflow."), this);
    explanation->setWordWrap(true); root->addWidget(explanation);
    m_progress = new QLabel(this); m_statistics = new QLabel(this);
    root->addWidget(m_progress); root->addWidget(m_statistics);

    auto* auditBox = new QGroupBox(QStringLiteral("Audit evidence"), this);
    auto* form = new QFormLayout(auditBox);
    auto add = [form, auditBox](const QString& caption, QLabel*& label) {
        label = new QLabel(auditBox); label->setWordWrap(true); label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(caption, label);
    };
    add(QStringLiteral("Part:"), m_part); add(QStringLiteral("Description:"), m_description);
    add(QStringLiteral("Stored Color:"), m_storedColor); add(QStringLiteral("Known valid Color(s):"), m_knownColors);
    add(QStringLiteral("Suggested:"), m_suggested); add(QStringLiteral("Quantity:"), m_quantity);
    add(QStringLiteral("Storage:"), m_storage); add(QStringLiteral("Workspace:"), m_workspace);
    add(QStringLiteral("Confidence:"), m_confidence); add(QStringLiteral("Audit reason:"), m_reason);
    add(QStringLiteral("Inventory record ID:"), m_recordId); root->addWidget(auditBox);

    auto* currentBox = new QGroupBox(QStringLiteral("Current authoritative Inventory state"), this);
    auto* currentLayout = new QVBoxLayout(currentBox); m_currentState = new QLabel(currentBox);
    m_currentState->setWordWrap(true); currentLayout->addWidget(m_currentState); root->addWidget(currentBox);

    auto* noteForm = new QFormLayout;
    m_note = new QLineEdit(this); m_note->setMaxLength(1024);
    noteForm->addRow(QStringLiteral("Review note (optional):"), m_note); root->addLayout(noteForm);

    auto* editRow = new QHBoxLayout;
    m_edit = new QPushButton(QStringLiteral("Edit Inventory Record"), this);
    m_notFound = new QPushButton(QStringLiteral("Mark Not Found"), this);
    editRow->addWidget(m_edit); editRow->addWidget(m_notFound); editRow->addStretch(); root->addLayout(editRow);
    auto* actionRow = new QHBoxLayout;
    m_previous = new QPushButton(QStringLiteral("Previous"), this);
    m_next = new QPushButton(QStringLiteral("Next"), this);
    auto* fixed = new QPushButton(QStringLiteral("Mark Fixed"), this);
    auto* verified = new QPushButton(QStringLiteral("Verified OK"), this);
    auto* skipped = new QPushButton(QStringLiteral("Skip"), this);
    auto* close = new QPushButton(QStringLiteral("Close"), this);
    actionRow->addWidget(m_previous); actionRow->addWidget(m_next); actionRow->addStretch();
    actionRow->addWidget(fixed); actionRow->addWidget(verified); actionRow->addWidget(skipped); actionRow->addWidget(close);
    root->addLayout(actionRow);

    connect(m_previous, &QPushButton::clicked, this, [this] { navigate(-1); });
    connect(m_next, &QPushButton::clicked, this, [this] { navigate(1); });
    connect(m_edit, &QPushButton::clicked, this, &InventoryColorAuditReviewDialog::openEditor);
    connect(m_notFound, &QPushButton::clicked, this, [this] { dispose(QStringLiteral("NOT_FOUND")); });
    connect(fixed, &QPushButton::clicked, this, [this] { dispose(QStringLiteral("FIXED")); });
    connect(verified, &QPushButton::clicked, this, [this] { dispose(QStringLiteral("VERIFIED_OK")); });
    connect(skipped, &QPushButton::clicked, this, [this] { dispose(QStringLiteral("SKIPPED")); });
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    int initial = m_csv.firstPendingRow();
    if (initial < 0 && !m_reviewRows.isEmpty()) initial = m_reviewRows.front();
    showRow(initial);
    qDebug() << "Inventory Color Audit CSV opened" << m_csv.fileName()
             << "review rows" << m_reviewRows.size();
}

QString InventoryColorAuditReviewDialog::field(const QString& name) const { return m_csv.value(m_rowIndex, name); }
void InventoryColorAuditReviewDialog::setValue(QLabel* label, const QString& value)
{ label->setText(value.trimmed().isEmpty() ? QStringLiteral("—") : value); }

void InventoryColorAuditReviewDialog::showRow(int rowIndex)
{
    m_rowIndex = rowIndex; m_position = m_reviewRows.indexOf(rowIndex);
    const bool valid = m_position >= 0;
    if (!valid) {
        m_progress->setText(QStringLiteral("No COLOR_MISMATCH rows are available."));
        m_currentState->setText(QStringLiteral("Nothing to review."));
        m_edit->setEnabled(false); m_notFound->setEnabled(false); m_previous->setEnabled(false); m_next->setEnabled(false);
        updateStatistics(); return;
    }
    m_progress->setText(QStringLiteral("Record %1 of %2 — review status: %3")
                            .arg(m_position + 1).arg(m_reviewRows.size()).arg(field(QStringLiteral("review_status"))));
    setValue(m_part, field(QStringLiteral("part_number"))); setValue(m_description, field(QStringLiteral("part_description")));
    setValue(m_storedColor, field(QStringLiteral("stored_color_name"))); setValue(m_knownColors, field(QStringLiteral("known_colors")));
    QString suggestion = field(QStringLiteral("suggested_color_name"));
    if (!field(QStringLiteral("suggested_rebrickable_color_id")).isEmpty()) suggestion += QStringLiteral(" [%1]").arg(field(QStringLiteral("suggested_rebrickable_color_id")));
    setValue(m_suggested, suggestion); setValue(m_quantity, field(QStringLiteral("quantity")));
    setValue(m_storage, field(QStringLiteral("storage_path"))); setValue(m_workspace, field(QStringLiteral("workspace")));
    setValue(m_confidence, field(QStringLiteral("confidence"))); setValue(m_reason, field(QStringLiteral("reason")));
    setValue(m_recordId, field(QStringLiteral("inventory_record_id"))); m_note->setText(field(QStringLiteral("review_note")));
    m_previous->setEnabled(m_position > 0); m_next->setEnabled(m_position + 1 < m_reviewRows.size());
    refreshAuthoritativeState(); updateStatistics();
}

void InventoryColorAuditReviewDialog::refreshAuthoritativeState()
{
    bool idOk = false; const int id = field(QStringLiteral("inventory_record_id")).toInt(&idOk);
    const auto record = idOk ? InventoryRecordRepository().getById(id) : std::nullopt;
    m_recordExists = record.has_value(); m_notFound->setEnabled(!m_recordExists);
    if (!record) { m_currentState->setText(QStringLiteral("Record not found. It may be marked NOT_FOUND without ending the review.")); m_edit->setEnabled(false); return; }
    const auto part = PartRepository().getById(record->partId()); const auto color = ColorRepository().getById(record->colorId());
    const auto storage = StorageLocationRepository().getById(record->storageLocationId());
    const auto workspace = WorkspaceRepository().getById(record->workspaceId());
    const QString currentPart = part ? part->partNumber() : QStringLiteral("Unknown Part");
    const QString currentColor = color ? color->name() : QStringLiteral("Unknown Color");
    const bool partChanged = currentPart.compare(field(QStringLiteral("part_number")), Qt::CaseInsensitive) != 0;
    bool colorChanged = currentColor.compare(field(QStringLiteral("stored_color_name")), Qt::CaseInsensitive) != 0;
    bool rbOk = false; const int auditedRb = field(QStringLiteral("stored_rebrickable_color_id")).toInt(&rbOk);
    if (rbOk && color) colorChanged = color->rebrickableId() != auditedRb;
    const bool workspaceMatches = record->workspaceId() == m_workspaceContext.currentWorkspaceId();
    QString text = QStringLiteral("Part: %1; Color: %2; Quantity: %3; Storage: %4; Workspace: %5")
                       .arg(currentPart, currentColor).arg(record->quantity())
                       .arg(storage ? storage->name() : QStringLiteral("Unknown"), workspace ? workspace->name() : QStringLiteral("Unknown"));
    if (partChanged || colorChanged) text += QStringLiteral("\nThis Inventory record has already changed since the audit.");
    if (!workspaceMatches) text += QStringLiteral("\nSwitch to this record's Workspace before editing it.");
    m_currentState->setText(text); m_edit->setEnabled(workspaceMatches);
}

void InventoryColorAuditReviewDialog::navigate(int delta)
{ const int next = m_position + delta; if (next >= 0 && next < m_reviewRows.size()) showRow(m_reviewRows.at(next)); }

void InventoryColorAuditReviewDialog::openEditor()
{
    if (!m_recordExists) return;
    const int id = field(QStringLiteral("inventory_record_id")).toInt();
    emit statusMessageRequested(QStringLiteral("Opened Inventory record %1 for color review.").arg(id));
    EditInventoryDialog dialog(id, m_workspaceContext, this); dialog.exec(); refreshAuthoritativeState();
}

void InventoryColorAuditReviewDialog::dispose(const QString& status)
{
    QString error;
    if (!m_csv.saveDisposition(m_rowIndex, status, m_note->text(), &error)) {
        QMessageBox::critical(this, windowTitle(), error); return;
    }
    emit statusMessageRequested(QStringLiteral("Marked Inventory record %1 %2.")
                                    .arg(field(QStringLiteral("inventory_record_id")), status));
    const int next = m_csv.nextPendingRow(m_rowIndex);
    if (next >= 0) showRow(next);
    else {
        updateStatistics(); const auto stats = m_csv.statistics();
        const QString message = stats.skipped > 0
            ? QStringLiteral("No pending rows remain. %1 skipped row(s) remain available with Previous/Next.").arg(stats.skipped)
            : QStringLiteral("Inventory Color Audit review complete.");
        QMessageBox::information(this, windowTitle(), message);
        emit statusMessageRequested(message);
        showRow(m_rowIndex);
    }
}

void InventoryColorAuditReviewDialog::updateStatistics()
{
    const auto s = m_csv.statistics();
    m_statistics->setText(QStringLiteral("Total mismatches: %1   Pending: %2   Fixed: %3   Verified OK: %4   Skipped: %5   Not Found: %6")
                              .arg(s.total).arg(s.pending).arg(s.fixed).arg(s.verifiedOk).arg(s.skipped).arg(s.notFound));
}
