#include "MissingPartsExportDialog.h"

#include "../../services/builds/MissingPartsCsvWriter.h"
#include "../../services/builds/MissingPartsExportService.h"
#include "../../services/builds/PickABrickCsvWriter.h"
#include "../../services/builds/PickABrickExportService.h"
#include "../../settings/UserSettings.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

MissingPartsExportDialog::MissingPartsExportDialog(
    QList<MissingPartsExportRow> rows, QString defaultFileName, QWidget* parent,
    PartOverrideResolver partOverrideResolver)
    : QDialog(parent), m_rows(std::move(rows)),
      m_pickABrickRows(PickABrickExportService::createSourceRows(m_rows)),
      m_defaultFileName(std::move(defaultFileName)),
      m_partOverrideResolver(std::move(partOverrideResolver))
{
    setWindowTitle(QStringLiteral("Missing Parts Export"));
    resize(1050, 650);

    auto* root = new QVBoxLayout(this);
    auto* presetRow = new QHBoxLayout;
    presetRow->addWidget(new QLabel(QStringLiteral("Preset:"), this));
    m_preset = new QComboBox(this);
    m_preset->setObjectName(QStringLiteral("missingPartsExportPreset"));
    m_preset->addItem(QStringLiteral("General CSV"));
    m_preset->addItem(QStringLiteral("LEGO Pick a Brick"));
    presetRow->addWidget(m_preset);
    presetRow->addStretch(1);
    root->addLayout(presetRow);

    m_description = new QLabel(
        QStringLiteral("General CSV — choose fields and their order. The preview is exactly what will be exported."),
        this);
    m_description->setWordWrap(true);
    root->addWidget(m_description);

    auto* content = new QHBoxLayout;
    m_generalControls = new QWidget(this);
    m_generalControls->setObjectName(QStringLiteral("missingPartsGeneralControls"));
    auto* fieldLayout = new QVBoxLayout(m_generalControls);
    fieldLayout->setContentsMargins(0, 0, 0, 0);
    fieldLayout->addWidget(new QLabel(QStringLiteral("Export fields:"), this));
    m_fields = new QListWidget(this);
    m_fields->setSelectionMode(QAbstractItemView::SingleSelection);
    fieldLayout->addWidget(m_fields, 1);
    auto* ordering = new QHBoxLayout;
    m_moveUp = new QPushButton(QStringLiteral("Move Up"), this);
    m_moveDown = new QPushButton(QStringLiteral("Move Down"), this);
    ordering->addWidget(m_moveUp);
    ordering->addWidget(m_moveDown);
    fieldLayout->addLayout(ordering);
    auto* reset = new QPushButton(QStringLiteral("Reset to Defaults"), this);
    fieldLayout->addWidget(reset);
    content->addWidget(m_generalControls, 0);

    m_preview = new QTableWidget(this);
    m_preview->setObjectName(QStringLiteral("missingPartsExportPreview"));
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_preview->setSortingEnabled(false);
    m_preview->verticalHeader()->setVisible(false);
    content->addWidget(m_preview, 1);
    root->addLayout(content, 1);

    m_status = new QLabel(this);
    root->addWidget(m_status);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_export = buttons->addButton(QStringLiteral("Export..."), QDialogButtonBox::AcceptRole);
    root->addWidget(buttons);

    connect(m_fields, &QListWidget::itemChanged, this, [this] {
        persistConfiguration();
        updatePreview();
    });
    connect(m_fields, &QListWidget::currentRowChanged, this, [this] {
        const int row = m_fields->currentRow();
        m_moveUp->setEnabled(row > 0);
        m_moveDown->setEnabled(row >= 0 && row + 1 < m_fields->count());
    });
    connect(m_moveUp, &QPushButton::clicked, this, [this] { moveCurrentField(-1); });
    connect(m_moveDown, &QPushButton::clicked, this, [this] { moveCurrentField(1); });
    connect(reset, &QPushButton::clicked, this, [this] {
        applyConfiguration(MissingPartsExportService::defaultConfiguration());
        persistConfiguration();
        updatePreview();
    });
    connect(m_export, &QPushButton::clicked, this, &MissingPartsExportDialog::exportCsv);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });

    loadConfiguration();
    updatePreview();
}

void MissingPartsExportDialog::loadConfiguration()
{
    const auto& settings = UserSettings::instance();
    applyConfiguration(MissingPartsExportService::normalizeConfiguration(
        settings.missingPartsExportFieldOrder(),
        settings.missingPartsExportEnabledFields()));
}

void MissingPartsExportDialog::applyConfiguration(
    const MissingPartsExportConfiguration& configuration)
{
    QSignalBlocker blocker(m_fields);
    m_fields->clear();
    QHash<QString, MissingPartsExportFieldDescriptor> descriptors;
    for (const auto& descriptor : MissingPartsExportService::fieldDescriptors())
        descriptors.insert(descriptor.id, descriptor);
    for (const QString& id : configuration.fieldOrder) {
        if (!descriptors.contains(id)) continue;
        const auto descriptor = descriptors.value(id);
        auto* item = new QListWidgetItem(descriptor.displayName, m_fields);
        item->setData(Qt::UserRole, descriptor.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(configuration.enabledFields.contains(id)
                                ? Qt::Checked : Qt::Unchecked);
        if (descriptor.ambiguous)
            item->setToolTip(QStringLiteral("Multiple authoritative values are separated with semicolons."));
    }
    if (m_fields->count() > 0) m_fields->setCurrentRow(0);
}

MissingPartsExportConfiguration MissingPartsExportDialog::configuration() const
{
    MissingPartsExportConfiguration result;
    for (int row = 0; row < m_fields->count(); ++row) {
        const auto* item = m_fields->item(row);
        const QString id = item->data(Qt::UserRole).toString();
        result.fieldOrder.append(id);
        if (item->checkState() == Qt::Checked) result.enabledFields.insert(id);
    }
    return result;
}

void MissingPartsExportDialog::persistConfiguration()
{
    const auto current = configuration();
    UserSettings::instance().setMissingPartsExportConfiguration(
        current.fieldOrder, current.enabledFields.values());
}

void MissingPartsExportDialog::moveCurrentField(int offset)
{
    const int current = m_fields->currentRow();
    const int destination = current + offset;
    if (current < 0 || destination < 0 || destination >= m_fields->count()) return;
    QSignalBlocker blocker(m_fields);
    QListWidgetItem* item = m_fields->takeItem(current);
    m_fields->insertItem(destination, item);
    m_fields->setCurrentRow(destination);
    persistConfiguration();
    updatePreview();
}

void MissingPartsExportDialog::updatePreview()
{
    m_generalControls->setVisible(!pickABrickSelected());
    if (pickABrickSelected()) {
        m_description->setText(QStringLiteral(
            "LEGO Pick a Brick — review exact Element candidates, exclude unresolved rows if intended, and export elementId,quantity."));
        updatePickABrickPreview();
    } else {
        m_description->setText(QStringLiteral(
            "General CSV — choose fields and their order. The preview is exactly what will be exported."));
        updateGeneralPreview();
    }
}

bool MissingPartsExportDialog::pickABrickSelected() const
{
    return m_preset && m_preset->currentIndex() == 1;
}

void MissingPartsExportDialog::updateGeneralPreview()
{
    const auto projection = MissingPartsExportService::project(m_rows, configuration());
    m_preview->clear();
    m_preview->setColumnCount(projection.headers.size());
    m_preview->setHorizontalHeaderLabels(projection.headers);
    m_preview->setRowCount(projection.rows.size());
    for (int row = 0; row < projection.rows.size(); ++row) {
        for (int column = 0; column < projection.rows.at(row).size(); ++column)
            m_preview->setItem(row, column,
                               new QTableWidgetItem(projection.rows.at(row).at(column)));
    }
    m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_preview->horizontalHeader()->setStretchLastSection(!projection.headers.isEmpty());
    m_status->setText(QStringLiteral("%1 missing Part/Color row(s); %2 export field(s).")
                          .arg(m_rows.size()).arg(projection.fields.size()));
    m_export->setEnabled(!projection.fields.isEmpty() && !m_rows.isEmpty());
}

void MissingPartsExportDialog::updatePickABrickPreview()
{
    m_preview->clear();
    m_preview->setColumnCount(7);
    m_preview->setHorizontalHeaderLabels({QStringLiteral("Include"),
        QStringLiteral("Part Number"), QStringLiteral("Part Name"),
        QStringLiteral("Color"), QStringLiteral("Missing Qty"),
        QStringLiteral("Element ID"), QStringLiteral("Status")});
    m_preview->setRowCount(m_pickABrickRows.size());

    for (int rowIndex = 0; rowIndex < m_pickABrickRows.size(); ++rowIndex) {
        auto& row = m_pickABrickRows[rowIndex];
        auto* include = new QCheckBox(m_preview);
        include->setChecked(row.included);
        include->setToolTip(QStringLiteral(
            "Excluded rows are deliberately omitted from the Pick a Brick file."));
        m_preview->setCellWidget(rowIndex, 0, include);
        auto* partNumber = new QLineEdit(m_preview);
        partNumber->setObjectName(QStringLiteral("pickABrickPartOverride_%1").arg(rowIndex));
        partNumber->setText(row.hasPartOverride() ? row.overridePartNumber
                                                  : row.source.partNumber);
        partNumber->setToolTip(QStringLiteral("Original Part: %1 — %2\nEnter an exact BrickSuite Part number, or clear this field to reset.")
                                   .arg(row.source.partNumber, row.source.partName));
        m_preview->setCellWidget(rowIndex, 1, partNumber);
        m_preview->setItem(rowIndex, 2, new QTableWidgetItem(
            row.hasPartOverride() && !row.overridePartName.isEmpty()
                ? row.overridePartName : row.source.partName));
        m_preview->setItem(rowIndex, 3, new QTableWidgetItem(row.source.colorName));
        m_preview->setItem(rowIndex, 4,
                           new QTableWidgetItem(QString::number(row.source.missing)));

        auto* candidates = new QComboBox(m_preview);
        candidates->addItems(row.elementCandidates);
        const int selected = candidates->findText(row.selectedElementId);
        if (selected >= 0) candidates->setCurrentIndex(selected);
        candidates->setEnabled(!row.elementCandidates.isEmpty() && row.included);
        m_preview->setCellWidget(rowIndex, 5, candidates);

        auto* statusItem = new QTableWidgetItem(pickABrickRowStatus(row));
        m_preview->setItem(rowIndex, 6, statusItem);

        connect(partNumber, &QLineEdit::editingFinished, this,
                [this, rowIndex, partNumber] {
            if (!partNumber->isModified()) return;
            const QString requested = partNumber->text();
            partNumber->setModified(false);
            QTimer::singleShot(0, this, [this, rowIndex, requested] {
                resolvePartOverride(rowIndex, requested);
            });
        });

        connect(include, &QCheckBox::toggled, this,
                [this, rowIndex, candidates, statusItem](bool checked) {
            m_pickABrickRows[rowIndex].included = checked;
            candidates->setEnabled(checked
                && !m_pickABrickRows[rowIndex].elementCandidates.isEmpty());
            statusItem->setText(pickABrickRowStatus(m_pickABrickRows[rowIndex]));
            updatePickABrickSummary();
        });
        connect(candidates, &QComboBox::currentTextChanged, this,
                [this, rowIndex, statusItem](const QString& elementId) {
                    m_pickABrickRows[rowIndex].selectedElementId = elementId;
                    statusItem->setText(pickABrickRowStatus(m_pickABrickRows[rowIndex]));
                    updatePickABrickSummary();
                });
    }
    m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_preview->horizontalHeader()->setStretchLastSection(true);
    updatePickABrickSummary();
}

void MissingPartsExportDialog::updatePickABrickSummary()
{
    const auto projection = PickABrickExportService::project(m_pickABrickRows);
    m_status->setText(QStringLiteral(
        "%1 source row(s) included; %2 unresolved; %3 excluded (%4 piece(s)); %5 final Element row(s); %6 included piece(s).")
        .arg(projection.includedSourceRows)
        .arg(projection.unresolvedIncludedRows)
        .arg(projection.excludedSourceRows)
        .arg(projection.excludedPieces)
        .arg(projection.rows.size())
        .arg(projection.includedPieces));
    m_export->setEnabled(projection.ready());
}

QString MissingPartsExportDialog::pickABrickRowStatus(
    const PickABrickExportSourceRow& row) const
{
    if (!row.included) return QStringLiteral("Excluded");
    using State = PickABrickExportSourceRow::OverrideState;
    if (row.overrideState == State::Resolving) return QStringLiteral("Resolving Part override...");
    if (row.overrideState == State::PartNotFound) return QStringLiteral("Unresolved — Part not found");
    if (row.overrideState == State::NoElement) return QStringLiteral("Unresolved — no exact Element ID");
    if (row.overrideState == State::Unavailable) return QStringLiteral("Unresolved — Host override unavailable");
    if (row.overrideState == State::Failed) return QStringLiteral("Unresolved — Part override failed");
    if (row.elementCandidates.isEmpty()) return QStringLiteral("Unresolved — no exact Element ID");
    if (!PickABrickExportService::isValidElementId(row.selectedElementId))
        return QStringLiteral("Unresolved — no valid decimal Element ID");
    if (row.hasPartOverride()) {
        return row.elementCandidates.size() == 1
            ? QStringLiteral("Ready — Part override")
            : QStringLiteral("Multiple IDs — Part override selected");
    }
    return row.elementCandidates.size() == 1
        ? QStringLiteral("Ready")
        : QStringLiteral("Multiple IDs — suggested selection");
}

void MissingPartsExportDialog::resolvePartOverride(int rowIndex,
                                                    const QString& partNumber)
{
    if (rowIndex < 0 || rowIndex >= m_pickABrickRows.size()) return;
    auto& row = m_pickABrickRows[rowIndex];
    const QString requested = partNumber.trimmed();
    if (requested.isEmpty() || requested == row.source.partNumber) {
        ++row.resolutionGeneration;
        row.overridePartNumber.clear();
        row.overridePartName.clear();
        row.overrideState = PickABrickExportSourceRow::OverrideState::None;
        row.elementCandidates = PickABrickExportService::numericCandidateOrder(
            row.source.pickABrickElementCandidates);
        row.selectedElementId = PickABrickExportService::suggestedElementId(
            row.elementCandidates);
        updatePickABrickPreview();
        return;
    }

    row.overridePartNumber = requested;
    row.overridePartName.clear();
    row.elementCandidates.clear();
    row.selectedElementId.clear();
    const quint64 generation = ++row.resolutionGeneration;
    if (!m_partOverrideResolver) {
        row.overrideState = PickABrickExportSourceRow::OverrideState::Unavailable;
        updatePickABrickPreview();
        return;
    }

    row.overrideState = PickABrickExportSourceRow::OverrideState::Resolving;
    updatePickABrickPreview();
    m_partOverrideResolver(requested, row.source.rebrickableColorId, this,
        [this, rowIndex, generation](const PickABrickPartResolution& resolution) {
            if (rowIndex < 0 || rowIndex >= m_pickABrickRows.size()) return;
            auto& current = m_pickABrickRows[rowIndex];
            if (current.resolutionGeneration != generation) return;
            if (!resolution.serviceAvailable) {
                current.overrideState = PickABrickExportSourceRow::OverrideState::Failed;
            } else if (!resolution.partFound) {
                current.overrideState = PickABrickExportSourceRow::OverrideState::PartNotFound;
            } else {
                current.overridePartNumber = resolution.partNumber;
                current.overridePartName = resolution.partName;
                current.elementCandidates = PickABrickExportService::numericCandidateOrder(
                    resolution.elementCandidates);
                current.selectedElementId = PickABrickExportService::suggestedElementId(
                    current.elementCandidates);
                current.overrideState = current.elementCandidates.isEmpty()
                    ? PickABrickExportSourceRow::OverrideState::NoElement
                    : PickABrickExportSourceRow::OverrideState::Ready;
            }
            updatePickABrickPreview();
        });
}

void MissingPartsExportDialog::exportCsv()
{
    if (pickABrickSelected()) {
        const auto projection = PickABrickExportService::project(m_pickABrickRows);
        if (!projection.ready()) {
            QMessageBox::warning(this, QStringLiteral("Export Missing Parts"),
                projection.error.isEmpty()
                    ? QStringLiteral("Resolve or exclude every included row before exporting.")
                    : projection.error);
            return;
        }
        QString targetName = m_defaultFileName;
        if (targetName.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
            targetName.chop(4);
        targetName += QStringLiteral("_PickABrick.csv");
        const QString fileName = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export LEGO Pick a Brick CSV"), targetName,
            QStringLiteral("CSV Files (*.csv)"));
        if (fileName.isEmpty()) return;
        const auto result = PickABrickCsvWriter::write(fileName, projection);
        if (!result.success) {
            QMessageBox::critical(this, QStringLiteral("Export Missing Parts"), result.message);
            return;
        }
        QMessageBox::information(this, QStringLiteral("Export Missing Parts"),
            QStringLiteral("LEGO Pick a Brick CSV exported successfully.\n\nElement Rows: %1\nPieces: %2\nFile:\n%3")
                .arg(projection.rows.size()).arg(projection.includedPieces).arg(fileName));
        accept();
        return;
    }
    const auto projection = MissingPartsExportService::project(m_rows, configuration());
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Missing Parts CSV"), m_defaultFileName,
        QStringLiteral("CSV Files (*.csv)"));
    if (fileName.isEmpty()) return;
    const auto result = MissingPartsCsvWriter::write(fileName, projection);
    if (!result.success) {
        QMessageBox::critical(this, QStringLiteral("Export Missing Parts"), result.message);
        return;
    }
    int pieces = 0;
    for (const auto& row : m_rows) pieces += row.missing;
    QMessageBox::information(this, QStringLiteral("Export Missing Parts"),
        QStringLiteral("Missing Parts List exported successfully.\n\nPart/Color Rows: %1\nPieces Missing: %2\nFile:\n%3")
            .arg(m_rows.size()).arg(pieces).arg(fileName));
    accept();
}
