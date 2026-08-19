/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkPartMappingTestDialog.h"

#include "../../api/ApiProvider.h"
#include "../../models/ExternalPartMapping.h"
#include "../../repositories/ExternalPartMappingRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/mappings/BrickLinkPartResolver.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

BrickLinkPartMappingTestDialog::BrickLinkPartMappingTestDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("BrickLink Part Mapping Resolver"));
    resize(620, 420);

    auto* mainLayout = new QVBoxLayout(this);

    auto* noteLabel = new QLabel(
        QStringLiteral("BrickLink API access is not used. Normal parts resolve "
                       "directly from the BrickSuite/Rebrickable part number. "
                       "Only exceptions require a stored mapping."),
        this);
    noteLabel->setWordWrap(true);
    mainLayout->addWidget(noteLabel);

    auto* form = new QFormLayout();

    auto* partRow = new QWidget(this);
    auto* partRowLayout = new QHBoxLayout(partRow);
    partRowLayout->setContentsMargins(0, 0, 0, 0);

    m_partNumberEdit = new QLineEdit(partRow);
    m_partNumberEdit->setPlaceholderText(QStringLiteral("Example: 3001"));

    m_loadButton = new QPushButton(QStringLiteral("Load"), partRow);

    partRowLayout->addWidget(m_partNumberEdit, 1);
    partRowLayout->addWidget(m_loadButton);

    m_partNameLabel = new QLabel(QStringLiteral("-"), this);
    m_itemIdLabel = new QLabel(QStringLiteral("-"), this);
    m_statusLabel = new QLabel(QStringLiteral("-"), this);
    m_canExportLabel = new QLabel(QStringLiteral("-"), this);
    m_messageLabel = new QLabel(QStringLiteral("-"), this);
    m_messageLabel->setWordWrap(true);

    form->addRow(QStringLiteral("BrickSuite Part:"), partRow);
    form->addRow(QStringLiteral("Part Name:"), m_partNameLabel);
    form->addRow(QStringLiteral("BrickLink ITEMID:"), m_itemIdLabel);
    form->addRow(QStringLiteral("Resolution:"), m_statusLabel);
    form->addRow(QStringLiteral("Can Export:"), m_canExportLabel);
    form->addRow(QStringLiteral("Message:"), m_messageLabel);

    mainLayout->addLayout(form);

    auto* exceptionGroup = new QWidget(this);
    auto* exceptionLayout = new QFormLayout(exceptionGroup);

    m_overrideEdit = new QLineEdit(exceptionGroup);
    m_overrideEdit->setPlaceholderText(
        QStringLiteral("Example override: 3245b"));

    exceptionLayout->addRow(QStringLiteral("Test Override:"), m_overrideEdit);

    auto* actionRow = new QWidget(exceptionGroup);
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);

    m_saveOverrideButton = new QPushButton(QStringLiteral("Save Override"), actionRow);
    m_unknownButton = new QPushButton(QStringLiteral("Mark Unknown"), actionRow);
    m_unsupportedButton = new QPushButton(QStringLiteral("Mark Unsupported"), actionRow);
    m_clearButton = new QPushButton(QStringLiteral("Clear Mapping"), actionRow);

    actionLayout->addWidget(m_saveOverrideButton);
    actionLayout->addWidget(m_unknownButton);
    actionLayout->addWidget(m_unsupportedButton);
    actionLayout->addWidget(m_clearButton);

    exceptionLayout->addRow(QString(), actionRow);

    mainLayout->addWidget(exceptionGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    connect(m_loadButton,
            &QPushButton::clicked,
            this,
            &BrickLinkPartMappingTestDialog::loadPart);

    connect(m_partNumberEdit,
            &QLineEdit::returnPressed,
            this,
            &BrickLinkPartMappingTestDialog::loadPart);

    connect(m_saveOverrideButton,
            &QPushButton::clicked,
            this,
            &BrickLinkPartMappingTestDialog::saveMappedOverride);

    connect(m_unknownButton,
            &QPushButton::clicked,
            this,
            &BrickLinkPartMappingTestDialog::markUnknown);

    connect(m_unsupportedButton,
            &QPushButton::clicked,
            this,
            &BrickLinkPartMappingTestDialog::markUnsupported);

    connect(m_clearButton,
            &QPushButton::clicked,
            this,
            &BrickLinkPartMappingTestDialog::clearMapping);
}

void BrickLinkPartMappingTestDialog::loadPart()
{
    const QString partNumber = m_partNumberEdit->text().trimmed();

    PartRepository partRepository;
    const auto part = partRepository.getByPartNumber(partNumber);

    if (!part) {
        m_partId = 0;
        m_partNameLabel->setText(QStringLiteral("Not found"));
        m_itemIdLabel->setText(QStringLiteral("-"));
        m_statusLabel->setText(QStringLiteral("Needs Review"));
        m_canExportLabel->setText(QStringLiteral("No"));
        m_messageLabel->setText(
            QStringLiteral("The part number was not found in BrickSuite."));
        return;
    }

    m_partId = part->id();
    m_partNameLabel->setText(part->name());

    refreshResolution();
}

void BrickLinkPartMappingTestDialog::refreshResolution()
{
    if (m_partId <= 0)
        return;

    BrickLinkPartResolver resolver;

    const auto result =
        resolver.resolve(m_partId, m_partNumberEdit->text().trimmed());

    m_itemIdLabel->setText(
        result.itemId.isEmpty() ? QStringLiteral("-") : result.itemId);

    m_statusLabel->setText(
        BrickLinkPartResolver::statusText(result.status));

    m_canExportLabel->setText(
        result.canExport ? QStringLiteral("Yes") : QStringLiteral("No"));

    m_messageLabel->setText(result.message);
}

void BrickLinkPartMappingTestDialog::saveMappedOverride()
{
    if (m_partId <= 0) {
        QMessageBox::warning(this,
                             QStringLiteral("BrickLink Part Mapping"),
                             QStringLiteral("Load a BrickSuite part first."));
        return;
    }

    const QString externalId = m_overrideEdit->text().trimmed();

    if (externalId.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("BrickLink Part Mapping"),
                             QStringLiteral("Enter a BrickLink ITEMID override first."));
        return;
    }

    ExternalPartMapping mapping;
    mapping.partId = m_partId;
    mapping.provider = apiProviderName(ApiProvider::BrickLink);
    mapping.externalId = externalId;
    mapping.status = ExternalMappingStatus::Mapped;
    mapping.source = QStringLiteral("User");
    mapping.notes = QStringLiteral("User-confirmed BrickLink part-number override.");

    ExternalPartMappingRepository repository;

    if (!repository.upsert(mapping)) {
        QMessageBox::critical(this,
                              QStringLiteral("BrickLink Part Mapping"),
                              QStringLiteral("Unable to save the override."));
        return;
    }

    refreshResolution();
}

void BrickLinkPartMappingTestDialog::markUnknown()
{
    if (m_partId <= 0)
        return;

    ExternalPartMapping mapping;
    mapping.partId = m_partId;
    mapping.provider = apiProviderName(ApiProvider::BrickLink);
    mapping.status = ExternalMappingStatus::Unknown;
    mapping.source = QStringLiteral("User");
    mapping.notes = QStringLiteral("BrickLink ITEMID requires review.");

    ExternalPartMappingRepository repository;
    repository.upsert(mapping);

    refreshResolution();
}

void BrickLinkPartMappingTestDialog::markUnsupported()
{
    if (m_partId <= 0)
        return;

    ExternalPartMapping mapping;
    mapping.partId = m_partId;
    mapping.provider = apiProviderName(ApiProvider::BrickLink);
    mapping.status = ExternalMappingStatus::Unsupported;
    mapping.source = QStringLiteral("User");
    mapping.notes = QStringLiteral("Part marked unsupported for BrickLink export.");

    ExternalPartMappingRepository repository;
    repository.upsert(mapping);

    refreshResolution();
}

void BrickLinkPartMappingTestDialog::clearMapping()
{
    if (m_partId <= 0)
        return;

    ExternalPartMappingRepository repository;

    if (!repository.remove(m_partId,
                           apiProviderName(ApiProvider::BrickLink))) {
        QMessageBox::critical(this,
                              QStringLiteral("BrickLink Part Mapping"),
                              QStringLiteral("Unable to clear the mapping."));
        return;
    }

    m_overrideEdit->clear();
    refreshResolution();
}
