/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#include "AddPartReferenceDialog.h"
#include "../../repositories/PartRepository.h"
#include "../../services/parts/PartReferenceCustomizationService.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AddPartReferenceDialog::AddPartReferenceDialog(int initialPartId,
                                               const PartReferenceEntry* anchor,
                                               QWidget* parent)
    : QDialog(parent), m_initialPartId(initialPartId)
{
    setWindowTitle(tr("Add Part to Reference")); resize(620, 520);
    if (anchor) { m_defaultCatalog = anchor->catalog; m_defaultSection = anchor->section;
                  m_defaultAnchor = anchor->partNumber; }
    QString error;
    if (!m_manifest.load(&error)) { QMessageBox::critical(this, windowTitle(), error); return; }
    PartReferenceCustomizationService service(m_manifest);
    m_effective = service.effectiveEntries();

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_search = new QLineEdit(this); m_search->setPlaceholderText(tr("Part number or name"));
    form->addRow(tr("Find Part:"), m_search);
    m_results = new QListWidget(this); m_results->setMinimumHeight(160);
    form->addRow(tr("Catalog Parts:"), m_results);
    m_destination = new QComboBox(this);
    for (const auto& destination : service.destinations()) {
        m_destination->addItem(QStringLiteral("%1 — %2").arg(destination.catalog, destination.section),
                               QStringList{destination.catalog, destination.section});
        if (destination.catalog == m_defaultCatalog && destination.section == m_defaultSection)
            m_destination->setCurrentIndex(m_destination->count() - 1);
    }
    form->addRow(tr("Family:"), m_destination);
    m_placement = new QComboBox(this);
    m_placement->addItem(tr("Append"), static_cast<int>(PartReferencePlacement::Append));
    m_placement->addItem(tr("Before selected"), static_cast<int>(PartReferencePlacement::Before));
    m_placement->addItem(tr("After selected"), static_cast<int>(PartReferencePlacement::After));
    if (!m_defaultAnchor.isEmpty()) m_placement->setCurrentIndex(2);
    form->addRow(tr("Placement:"), m_placement);
    m_anchor = new QComboBox(this); form->addRow(tr("Relative to:"), m_anchor);
    layout->addLayout(form);
    m_note = new QLabel(this); m_note->setWordWrap(true); layout->addWidget(m_note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_save = buttons->button(QDialogButtonBox::Save); layout->addWidget(buttons);
    connect(m_search, &QLineEdit::textChanged, this, &AddPartReferenceDialog::searchParts);
    connect(m_destination, &QComboBox::currentIndexChanged, this, &AddPartReferenceDialog::destinationChanged);
    connect(m_placement, &QComboBox::currentIndexChanged, this, &AddPartReferenceDialog::destinationChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddPartReferenceDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (initialPartId > 0) {
        const auto part = PartRepository().getById(initialPartId);
        if (part) m_search->setText(part->partNumber());
    } else searchParts();
    destinationChanged();
}

void AddPartReferenceDialog::searchParts()
{
    m_results->clear();
    const QList<Part> parts = PartRepository().searchForInventoryEntry(m_search->text(), 50);
    for (const Part& part : parts) {
        auto* item = new QListWidgetItem(QStringLiteral("%1 — %2").arg(part.partNumber(), part.name()), m_results);
        item->setData(Qt::UserRole, part.id());
        if (part.id() == m_initialPartId) m_results->setCurrentItem(item);
    }
    if (!m_results->currentItem() && m_results->count() == 1) m_results->setCurrentRow(0);
}

int AddPartReferenceDialog::selectedPartId() const
{ return m_results->currentItem() ? m_results->currentItem()->data(Qt::UserRole).toInt() : 0; }

void AddPartReferenceDialog::destinationChanged()
{
    const QStringList destination = m_destination->currentData().toStringList();
    if (destination.size() != 2) return;
    const bool structured = PartReferenceCustomizationService::isStructuredCatalog(destination.at(0));
    if (structured) m_placement->setCurrentIndex(0);
    m_placement->setEnabled(!structured);
    m_note->setText(structured ? tr("This structured catalog keeps its dimension matrix protected. "
                                    "The Part will be appended in the Other gallery.") : QString());
    m_anchor->clear();
    for (const PartReferenceEntry& entry : m_effective) {
        if (entry.catalog == destination.at(0) && entry.section == destination.at(1)) {
            m_anchor->addItem(QStringLiteral("%1 — %2").arg(entry.partNumber, entry.partName), entry.partNumber);
            if (entry.partNumber.compare(m_defaultAnchor, Qt::CaseInsensitive) == 0)
                m_anchor->setCurrentIndex(m_anchor->count() - 1);
        }
    }
    const auto placement = static_cast<PartReferencePlacement>(m_placement->currentData().toInt());
    m_anchor->setEnabled(!structured && placement != PartReferencePlacement::Append);
}

void AddPartReferenceDialog::save()
{
    const int partId = selectedPartId();
    if (partId <= 0) { QMessageBox::warning(this, windowTitle(), tr("Select a catalog Part.")); return; }
    const QStringList destination = m_destination->currentData().toStringList();
    if (destination.size() != 2) return;
    const auto placement = static_cast<PartReferencePlacement>(m_placement->currentData().toInt());
    const auto result = PartReferenceCustomizationService(m_manifest).add(
        partId, destination.at(0), destination.at(1), placement, m_anchor->currentData().toString());
    if (!result.success) { QMessageBox::warning(this, windowTitle(), result.message); return; }
    m_added = true; accept();
}
