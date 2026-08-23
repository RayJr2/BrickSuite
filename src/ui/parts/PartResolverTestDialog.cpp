/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "PartResolverTestDialog.h"

#include "../../models/PartResolutionResult.h"
#include "../../services/parts/PartResolver.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PartResolverTestDialog::PartResolverTestDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Part Identity Resolver"));
    resize(760, 520);

    auto* mainLayout = new QVBoxLayout(this);

    auto* noteLabel = new QLabel(
        QStringLiteral("Tests the BrickSuite central Part Resolver. Exact catalog "
                       "matches always win. Active aliases may resolve to a canonical "
                       "part. Rebrickable Alternate/Mold relationships are advisory "
                       "and are never silently substituted."),
        this);
    noteLabel->setWordWrap(true);
    mainLayout->addWidget(noteLabel);

    auto* form = new QFormLayout();

    auto* inputRow = new QWidget(this);
    auto* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);

    m_partNumberEdit = new QLineEdit(inputRow);
    m_partNumberEdit->setPlaceholderText(
        QStringLiteral("Enter BrickSuite or alias part number"));

    m_resolveButton =
        new QPushButton(QStringLiteral("Resolve"), inputRow);

    inputLayout->addWidget(m_partNumberEdit, 1);
    inputLayout->addWidget(m_resolveButton);

    m_statusLabel = new QLabel(QStringLiteral("-"), this);
    m_resolvedPartLabel = new QLabel(QStringLiteral("-"), this);
    m_partNameLabel = new QLabel(QStringLiteral("-"), this);
    m_aliasLabel = new QLabel(QStringLiteral("-"), this);
    m_messageLabel = new QLabel(QStringLiteral("-"), this);
    m_messageLabel->setWordWrap(true);

    form->addRow(QStringLiteral("Input Part:"), inputRow);
    form->addRow(QStringLiteral("Resolution:"), m_statusLabel);
    form->addRow(QStringLiteral("Resolved Part:"), m_resolvedPartLabel);
    form->addRow(QStringLiteral("Part Name:"), m_partNameLabel);
    form->addRow(QStringLiteral("Alias Source:"), m_aliasLabel);
    form->addRow(QStringLiteral("Message:"), m_messageLabel);

    mainLayout->addLayout(form);

    auto* candidatesLabel =
        new QLabel(QStringLiteral("Alternate / Mold Relationship Candidates"), this);
    mainLayout->addWidget(candidatesLabel);

    m_candidatesTable = new QTableWidget(this);
    m_candidatesTable->setColumnCount(4);
    m_candidatesTable->setHorizontalHeaderLabels(
        QStringList()
        << QStringLiteral("Part #")
        << QStringLiteral("Name")
        << QStringLiteral("Relationship")
        << QStringLiteral("Source"));

    m_candidatesTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers);
    m_candidatesTable->setSelectionBehavior(
        QAbstractItemView::SelectRows);
    m_candidatesTable->setSelectionMode(
        QAbstractItemView::SingleSelection);
    m_candidatesTable->verticalHeader()->setVisible(false);
    m_candidatesTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_candidatesTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    m_candidatesTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_candidatesTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);

    mainLayout->addWidget(m_candidatesTable, 1);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject);
    mainLayout->addWidget(buttons);

    connect(m_resolveButton,
            &QPushButton::clicked,
            this,
            &PartResolverTestDialog::resolvePart);

    connect(m_partNumberEdit,
            &QLineEdit::returnPressed,
            this,
            &PartResolverTestDialog::resolvePart);
}

void PartResolverTestDialog::resolvePart()
{
    PartResolver resolver;

    const PartResolutionResult result =
        resolver.resolve(m_partNumberEdit->text());

    m_statusLabel->setText(
        partResolutionStatusText(result.status));

    if (result.hasResolvedPart) {
        m_resolvedPartLabel->setText(
            result.part.partNumber());
        m_partNameLabel->setText(
            result.part.name());
    } else {
        m_resolvedPartLabel->setText(QStringLiteral("-"));
        m_partNameLabel->setText(QStringLiteral("-"));
    }

    if (result.matchedAlias) {
        m_aliasLabel->setText(
            QStringLiteral("%1 / %2")
                .arg(partAliasTypeToString(result.alias.aliasType),
                     result.alias.source));
    } else {
        m_aliasLabel->setText(QStringLiteral("-"));
    }

    m_messageLabel->setText(result.message);

    m_candidatesTable->setRowCount(0);

    int row = 0;

    for (const PartResolutionCandidate& candidate
         : result.relationshipCandidates) {
        m_candidatesTable->insertRow(row);

        m_candidatesTable->setItem(
            row,
            0,
            new QTableWidgetItem(candidate.part.partNumber()));

        m_candidatesTable->setItem(
            row,
            1,
            new QTableWidgetItem(candidate.part.name()));

        m_candidatesTable->setItem(
            row,
            2,
            new QTableWidgetItem(
                partRelationshipTypeToString(
                    candidate.relationshipType)));

        m_candidatesTable->setItem(
            row,
            3,
            new QTableWidgetItem(candidate.source));

        ++row;
    }
}
