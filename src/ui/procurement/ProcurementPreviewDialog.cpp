/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ProcurementPreviewDialog.h"

#include "../../api/ApiProvider.h"
#include "../../models/Color.h"
#include "../../models/ExternalColorMapping.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/ExternalColorMappingRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"
#include "../../models/ExternalPartMapping.h"
#include "../../services/procurement/BrickLinkWantedListXmlWriter.h"
#include "BrickLinkWantedListResultDialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
constexpr int PartNumberColumn = 0;
constexpr int PartNameColumn = 1;
constexpr int ItemIdColumn = 2;
constexpr int RememberPartColumn = 3;
constexpr int SourceColorColumn = 4;
constexpr int BrickLinkColorColumn = 5;
constexpr int QuantityColumn = 6;
constexpr int PartStatusColumn = 7;
constexpr int ColorStatusColumn = 8;
constexpr int ExportStatusColumn = 9;
}

ProcurementPreviewDialog::ProcurementPreviewDialog(
    const ProcurementDraft& draft,
    QWidget* parent)
    : QDialog(parent)
    , m_draft(draft)
{
    setWindowTitle(QStringLiteral("Missing Parts Procurement Preview"));
    resize(1220, 720);

    buildUi();
    populateRows();
    updateSummary();
}

const ProcurementDraft& ProcurementPreviewDialog::draft() const
{
    return m_draft;
}

BrickLinkWantedListOptions ProcurementPreviewDialog::brickLinkOptions() const
{
    BrickLinkWantedListOptions options;

    options.condition = m_conditionCombo->currentData().toString();
    options.notify = m_notifyCombo->currentData().toString();
    options.wantedShow = m_wantedShowCombo->currentData().toString();
    options.remarksMode = m_remarksCombo->currentData().toString();
    options.customRemarks = m_customRemarksEdit->text().trimmed();

    return options;
}

void ProcurementPreviewDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* heading = new QLabel(
        QStringLiteral("<b>Target: BrickLink Wanted List</b>"),
        this);
    mainLayout->addWidget(heading);

    m_buildLabel = new QLabel(this);
    m_buildLabel->setWordWrap(true);

    QString buildIdentity = m_draft.buildName;

    if (!m_draft.buildNumber.trimmed().isEmpty()) {
        buildIdentity =
            QStringLiteral("%1 — %2")
                .arg(m_draft.buildNumber.trimmed(), m_draft.buildName);
    }

    m_buildLabel->setText(
        QStringLiteral("Build: %1    Type: %2")
            .arg(buildIdentity, m_draft.buildType));
    mainLayout->addWidget(m_buildLabel);

    m_summaryLabel = new QLabel(this);
    mainLayout->addWidget(m_summaryLabel);

    auto* optionsGroup = new QGroupBox(
        QStringLiteral("BrickLink Wanted List Optional Fields"),
        this);
    auto* optionsLayout = new QFormLayout(optionsGroup);

    m_conditionCombo = new QComboBox(optionsGroup);
    m_conditionCombo->addItem(QStringLiteral("Do not include"), QString());
    m_conditionCombo->addItem(QStringLiteral("New"), QStringLiteral("N"));
    m_conditionCombo->addItem(QStringLiteral("Used"), QStringLiteral("U"));

    m_notifyCombo = new QComboBox(optionsGroup);
    m_notifyCombo->addItem(QStringLiteral("Do not include"), QString());
    m_notifyCombo->addItem(QStringLiteral("Yes"), QStringLiteral("Y"));
    m_notifyCombo->addItem(QStringLiteral("No"), QStringLiteral("N"));

    m_wantedShowCombo = new QComboBox(optionsGroup);
    m_wantedShowCombo->addItem(QStringLiteral("Do not include"), QString());
    m_wantedShowCombo->addItem(QStringLiteral("Yes"), QStringLiteral("Y"));
    m_wantedShowCombo->addItem(QStringLiteral("No"), QStringLiteral("N"));

    m_remarksCombo = new QComboBox(optionsGroup);
    m_remarksCombo->addItem(QStringLiteral("Do not include"), QString());
    m_remarksCombo->addItem(QStringLiteral("Use build name"),
                            QStringLiteral("BuildName"));
    m_remarksCombo->addItem(QStringLiteral("Custom..."),
                            QStringLiteral("Custom"));

    m_customRemarksEdit = new QLineEdit(optionsGroup);
    m_customRemarksEdit->setPlaceholderText(
        QStringLiteral("Remarks to include on each BrickLink item"));
    m_customRemarksEdit->setVisible(false);

    optionsLayout->addRow(QStringLiteral("Condition:"), m_conditionCombo);
    optionsLayout->addRow(QStringLiteral("Notify:"), m_notifyCombo);
    optionsLayout->addRow(QStringLiteral("Wanted Show:"), m_wantedShowCombo);
    optionsLayout->addRow(QStringLiteral("Remarks:"), m_remarksCombo);
    optionsLayout->addRow(QStringLiteral("Custom Remarks:"), m_customRemarksEdit);

    connect(m_remarksCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this]() {
                const bool custom =
                    m_remarksCombo->currentData().toString()
                    == QStringLiteral("Custom");
                m_customRemarksEdit->setVisible(custom);
            });

    mainLayout->addWidget(optionsGroup);

    auto* note = new QLabel(
        QStringLiteral("BrickLink ITEMID and color edits are session-only overrides. "
                       "For an ITEMID edit, check Remember to save it as a user-confirmed "
                       "BrickLink mapping when XML is generated. User mappings take precedence "
                       "over Rebrickable external IDs."),
        this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Part Number"),
        QStringLiteral("Part Name"),
        QStringLiteral("BrickLink ITEMID"),
        QStringLiteral("Remember"),
        QStringLiteral("BrickSuite Color"),
        QStringLiteral("BrickLink Color"),
        QStringLiteral("Missing Qty"),
        QStringLiteral("Part Status"),
        QStringLiteral("Color Status"),
        QStringLiteral("Export Status")
    });

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(
        PartNumberColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        PartNameColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(
        ItemIdColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        RememberPartColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        SourceColorColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        BrickLinkColorColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        QuantityColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        PartStatusColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        ColorStatusColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        ExportStatusColumn, QHeaderView::ResizeToContents);

    mainLayout->addWidget(m_table, 1);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Close, this);

    m_generateButton =
        new QPushButton(QStringLiteral("Generate BrickLink XML"), this);
    buttons->addButton(m_generateButton, QDialogButtonBox::ActionRole);

    connect(m_generateButton,
            &QPushButton::clicked,
            this,
            &ProcurementPreviewDialog::generateBrickLinkXml);

    connect(buttons,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject);

    mainLayout->addWidget(buttons);
}

QList<ProcurementPreviewDialog::BrickLinkColorChoice>
ProcurementPreviewDialog::loadMappedBrickLinkColors() const
{
    QList<BrickLinkColorChoice> choices;

    ExternalColorMappingRepository mappingRepository;
    ColorRepository colorRepository;

    const QList<ExternalColorMapping> mappings =
        mappingRepository.getByProvider(
            apiProviderName(ApiProvider::BrickLink));

    for (const ExternalColorMapping& mapping : mappings) {
        if (mapping.status != ExternalMappingStatus::Mapped
            || mapping.externalId.trimmed().isEmpty()) {
            continue;
        }

        const auto color = colorRepository.getById(mapping.colorId);

        if (!color)
            continue;

        BrickLinkColorChoice choice;
        choice.externalId = mapping.externalId.trimmed();
        choice.displayName =
            QStringLiteral("%1 (%2)")
                .arg(color->name(), choice.externalId);

        choices.append(choice);
    }

    std::sort(
        choices.begin(),
        choices.end(),
        [](const BrickLinkColorChoice& left,
           const BrickLinkColorChoice& right) {
            return left.displayName.compare(
                       right.displayName,
                       Qt::CaseInsensitive) < 0;
        });

    return choices;
}

void ProcurementPreviewDialog::populateRows()
{
    const QList<BrickLinkColorChoice> mappedColors =
        loadMappedBrickLinkColors();

    m_table->setRowCount(m_draft.items.size());

    for (int row = 0; row < m_draft.items.size(); ++row) {
        ProcurementItem& item = m_draft.items[row];

        m_table->setItem(
            row,
            PartNumberColumn,
            new QTableWidgetItem(item.partNumber));

        m_table->setItem(
            row,
            PartNameColumn,
            new QTableWidgetItem(item.partName));

        auto* itemIdEdit = new QLineEdit(m_table);
        itemIdEdit->setText(item.resolvedItemId);
        itemIdEdit->setPlaceholderText(
            QStringLiteral("Enter BrickLink ITEMID"));
        m_table->setCellWidget(row, ItemIdColumn, itemIdEdit);

        auto* rememberCheck = new QCheckBox(m_table);
        rememberCheck->setToolTip(
            QStringLiteral("Save an edited ITEMID as a user-confirmed BrickLink mapping "
                           "when XML is generated."));
        rememberCheck->setEnabled(false);
        rememberCheck->setChecked(false);

        auto* rememberContainer = new QWidget(m_table);
        auto* rememberLayout = new QHBoxLayout(rememberContainer);
        rememberLayout->setContentsMargins(0, 0, 0, 0);
        rememberLayout->setAlignment(Qt::AlignCenter);
        rememberLayout->addWidget(rememberCheck);

        m_table->setCellWidget(row, RememberPartColumn, rememberContainer);
        m_rememberPartOverrideChecks.append(rememberCheck);

        m_table->setItem(
            row,
            SourceColorColumn,
            new QTableWidgetItem(item.colorName));

        auto* colorCombo = new QComboBox(m_table);
        colorCombo->addItem(
            QStringLiteral("Unknown — Select BrickLink Color..."),
            QString());

        int selectedColorIndex = 0;

        for (const BrickLinkColorChoice& choice : mappedColors) {
            colorCombo->addItem(choice.displayName, choice.externalId);

            if (item.resolvedColorReady
                && choice.externalId == item.resolvedColorId) {
                selectedColorIndex = colorCombo->count() - 1;
            }
        }

        colorCombo->setCurrentIndex(selectedColorIndex);
        m_table->setCellWidget(row, BrickLinkColorColumn, colorCombo);

        m_table->setItem(
            row,
            QuantityColumn,
            new QTableWidgetItem(QString::number(item.quantityNeeded)));

        m_table->setItem(
            row,
            PartStatusColumn,
            new QTableWidgetItem(item.resolvedItemStatus));

        m_table->setItem(
            row,
            ColorStatusColumn,
            new QTableWidgetItem(item.resolvedColorStatus));

        m_table->setItem(
            row,
            ExportStatusColumn,
            new QTableWidgetItem(QString()));

        connect(itemIdEdit,
                &QLineEdit::textChanged,
                this,
                [this, row, rememberCheck](const QString& text) {
                    ProcurementItem& changed = m_draft.items[row];

                    const QString trimmed = text.trimmed();

                    if (trimmed == changed.resolvedItemId.trimmed()) {
                        changed.itemOverride.clear();
                        changed.rememberItemOverride = false;
                        rememberCheck->setChecked(false);
                        rememberCheck->setEnabled(false);
                    } else {
                        changed.itemOverride = trimmed;

                        const bool usableOverride = !trimmed.isEmpty();
                        rememberCheck->setEnabled(usableOverride);

                        // Remember is the normal behavior for a deliberate
                        // provider-ID correction, while still allowing the
                        // user to uncheck it for a one-time procurement edit.
                        if (usableOverride && !rememberCheck->isChecked())
                            rememberCheck->setChecked(true);
                    }

                    updatePartRow(row);
                    updateRowStatus(row);
                    updateSummary();
                });

        connect(rememberCheck,
                &QCheckBox::toggled,
                this,
                [this, row](bool checked) {
                    if (row < 0 || row >= m_draft.items.size())
                        return;

                    m_draft.items[row].rememberItemOverride = checked;
                });

        connect(colorCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, row, colorCombo](int) {
                    ProcurementItem& changed = m_draft.items[row];

                    const QString selectedId =
                        colorCombo->currentData().toString().trimmed();

                    if (!selectedId.isEmpty()
                        && changed.resolvedColorReady
                        && selectedId == changed.resolvedColorId) {
                        changed.colorOverrideId.clear();
                        changed.colorOverrideName.clear();
                    } else if (!selectedId.isEmpty()) {
                        changed.colorOverrideId = selectedId;
                        changed.colorOverrideName =
                            colorCombo->currentText();
                    } else {
                        changed.colorOverrideId.clear();
                        changed.colorOverrideName.clear();
                    }

                    updateColorRow(row);
                    updateRowStatus(row);
                    updateSummary();
                });

        updatePartRow(row);
        updateColorRow(row);
        updateRowStatus(row);
    }
}

void ProcurementPreviewDialog::updatePartRow(int row)
{
    if (row < 0 || row >= m_draft.items.size())
        return;

    const ProcurementItem& item = m_draft.items.at(row);

    QString status = item.resolvedItemStatus;

    if (!item.itemOverride.trimmed().isEmpty())
        status = QStringLiteral("Session Override");

    m_table->item(row, PartStatusColumn)->setText(status);
}

void ProcurementPreviewDialog::updateColorRow(int row)
{
    if (row < 0 || row >= m_draft.items.size())
        return;

    const ProcurementItem& item = m_draft.items.at(row);

    QString status = item.resolvedColorStatus;

    if (!item.colorOverrideId.trimmed().isEmpty())
        status = QStringLiteral("Override");

    m_table->item(row, ColorStatusColumn)->setText(status);
}

void ProcurementPreviewDialog::updateRowStatus(int row)
{
    if (row < 0 || row >= m_draft.items.size())
        return;

    const ProcurementItem& item = m_draft.items.at(row);

    m_table->item(row, ExportStatusColumn)
        ->setText(item.ready()
                      ? QStringLiteral("Ready")
                      : QStringLiteral("Needs Review"));
}

void ProcurementPreviewDialog::updateSummary()
{
    m_summaryLabel->setText(
        QStringLiteral(
            "Missing pieces: %1    Unique items: %2    "
            "Ready: %3    Needs Review: %4")
            .arg(m_draft.totalMissingPieces())
            .arg(m_draft.items.size())
            .arg(m_draft.readyRows())
            .arg(m_draft.reviewRows()));

    if (m_generateButton) {
        m_generateButton->setEnabled(
            !m_draft.items.isEmpty()
            && m_draft.reviewRows() == 0);
    }
}

bool ProcurementPreviewDialog::persistRememberedPartOverrides()
{
    ExternalPartMappingRepository repository;

    for (int row = 0; row < m_draft.items.size(); ++row) {
        ProcurementItem& item = m_draft.items[row];

        if (!item.rememberItemOverride
            || item.itemOverride.trimmed().isEmpty()) {
            continue;
        }

        ExternalPartMapping mapping;
        mapping.partId = item.partId;
        mapping.provider = apiProviderName(ApiProvider::BrickLink);
        mapping.externalId = item.itemOverride.trimmed();
        mapping.status = ExternalMappingStatus::Mapped;
        mapping.source = QStringLiteral("User");
        mapping.notes =
            QStringLiteral("User-confirmed BrickLink ITEMID from procurement preview.");

        if (!repository.upsert(mapping)) {
            QMessageBox::warning(
                this,
                QStringLiteral("BrickLink Wanted List"),
                QStringLiteral("Unable to save the BrickLink ITEMID override for part %1.")
                    .arg(item.partNumber));
            return false;
        }

        item.resolvedItemId = mapping.externalId;
        item.resolvedItemStatus = QStringLiteral("User Override");
        item.resolvedItemReady = true;
        item.itemOverride.clear();
        item.rememberItemOverride = false;

        if (row < m_rememberPartOverrideChecks.size()) {
            QCheckBox* check = m_rememberPartOverrideChecks.at(row);
            check->setChecked(false);
            check->setEnabled(false);
        }

        if (auto* edit =
                qobject_cast<QLineEdit*>(
                    m_table->cellWidget(row, ItemIdColumn))) {
            edit->setText(item.resolvedItemId);
        }

        updatePartRow(row);
        updateRowStatus(row);
    }

    return true;
}

void ProcurementPreviewDialog::generateBrickLinkXml()
{
    if (m_draft.items.isEmpty())
        return;

    if (m_draft.reviewRows() != 0) {
        QMessageBox::warning(
            this,
            QStringLiteral("BrickLink Wanted List"),
            QStringLiteral("Resolve every Needs Review row before generating BrickLink XML."));
        return;
    }

    if (!persistRememberedPartOverrides())
        return;

    BrickLinkWantedListXmlWriter writer;

    const BrickLinkWantedListXmlWriter::Result result =
        writer.write(m_draft, brickLinkOptions());

    if (!result.success) {
        QMessageBox::warning(
            this,
            QStringLiteral("BrickLink Wanted List"),
            result.message);
        return;
    }

    BrickLinkWantedListResultDialog dialog(
        result.xml,
        result.itemRows,
        result.totalPieces,
        m_draft.buildNumber,
        m_draft.buildName,
        this);

    dialog.exec();
}
