/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkColorMappingStatusDialog.h"

#include "../../api/ApiProvider.h"
#include "../../models/Color.h"
#include "../../models/ExternalColorMapping.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/ExternalColorMappingRepository.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
QString displayExternalId(const ExternalColorMapping& mapping)
{
    return mapping.externalId.trimmed().isEmpty()
        ? QStringLiteral("-")
        : mapping.externalId.trimmed();
}
}

BrickLinkColorMappingStatusDialog::BrickLinkColorMappingStatusDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("BrickLink Color Mapping Status"));
    resize(980, 620);

    auto* layout = new QVBoxLayout(this);

    auto* sourceLabel = new QLabel(
        QStringLiteral("BrickLink color IDs are resolved from Rebrickable reference data. "
                       "No BrickLink API access is used."),
        this);
    sourceLabel->setWordWrap(true);
    layout->addWidget(sourceLabel);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(QStringLiteral("Unknown"));
    m_filterCombo->addItem(QStringLiteral("All"));
    m_filterCombo->addItem(QStringLiteral("Mapped"));
    m_filterCombo->addItem(QStringLiteral("Unsupported"));
    layout->addWidget(m_filterCombo);

    m_summaryLabel = new QLabel(this);
    layout->addWidget(m_summaryLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Rebrickable ID"),
        QStringLiteral("BrickSuite Color"),
        QStringLiteral("BrickLink ID"),
        QStringLiteral("Status"),
        QStringLiteral("Source"),
        QStringLiteral("Reason")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_filterCombo,
            &QComboBox::currentTextChanged,
            this,
            [this]() {
                applyFilter();
                updateSummary();
            });

    reload();
}

void BrickLinkColorMappingStatusDialog::reload()
{
    ColorRepository colorRepository;
    ExternalColorMappingRepository mappingRepository;

    const QList<Color> colors = colorRepository.getAll();
    const QList<ExternalColorMapping> mappings =
        mappingRepository.getByProvider(apiProviderName(ApiProvider::BrickLink));

    QHash<int, ExternalColorMapping> mappingByColorId;
    for (const ExternalColorMapping& mapping : mappings)
        mappingByColorId.insert(mapping.colorId, mapping);

    m_table->setRowCount(0);

    for (const Color& color : colors) {
        ExternalColorMapping mapping;

        if (mappingByColorId.contains(color.id())) {
            mapping = mappingByColorId.value(color.id());
        } else {
            mapping.colorId = color.id();
            mapping.provider = apiProviderName(ApiProvider::BrickLink);
            mapping.status = ExternalMappingStatus::Unknown;
            mapping.notes = QStringLiteral("No BrickLink mapping record exists.");
        }

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* rebrickableItem =
            new QTableWidgetItem(QString::number(color.rebrickableId()));
        rebrickableItem->setData(Qt::UserRole,
                                 externalMappingStatusToString(mapping.status));

        m_table->setItem(row, 0, rebrickableItem);
        m_table->setItem(row, 1, new QTableWidgetItem(color.name()));
        m_table->setItem(row, 2, new QTableWidgetItem(displayExternalId(mapping)));
        m_table->setItem(
            row,
            3,
            new QTableWidgetItem(externalMappingStatusToString(mapping.status)));
        m_table->setItem(row, 4, new QTableWidgetItem(mapping.source));
        m_table->setItem(row, 5, new QTableWidgetItem(mapping.notes));
    }

    applyFilter();
    updateSummary();
}

void BrickLinkColorMappingStatusDialog::applyFilter()
{
    const QString filter = m_filterCombo->currentText();

    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString status =
            m_table->item(row, 0)->data(Qt::UserRole).toString();

        const bool visible =
            filter == QStringLiteral("All")
            || status.compare(filter, Qt::CaseInsensitive) == 0;

        m_table->setRowHidden(row, !visible);
    }
}

void BrickLinkColorMappingStatusDialog::updateSummary()
{
    int visibleRows = 0;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (!m_table->isRowHidden(row))
            ++visibleRows;
    }

    m_summaryLabel->setText(
        QStringLiteral("Showing %1 of %2 BrickSuite colors.")
            .arg(visibleRows)
            .arg(m_table->rowCount()));
}
