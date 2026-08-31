/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 */

#include "InteractiveBuildPullingDialog.h"

#include "../../services/RebrickableApiClient.h"
#include "../../services/images/PartImageService.h"
#include "../../settings/UserSettings.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int ImageColumn = 0;
constexpr int PartColumn = 1;
constexpr int DescriptionColumn = 2;
constexpr int ColorColumn = 3;
constexpr int RequiredColumn = 4;
constexpr int AlreadyPulledColumn = 5;
constexpr int AllocatedHereColumn = 6;
constexpr int PulledColumn = 7;
constexpr int StorageColumn = 8;
constexpr int ImageSize = 72;
constexpr int ImageBatchSize = 20;
}

InteractiveBuildPullingDialog::InteractiveBuildPullingDialog(int buildId,
                                                               QWidget* parent)
    : QDialog(parent),
      m_buildId(buildId)
{
    setWindowTitle(tr("Interactive Build Pulling"));
    resize(1180, 760);

    m_partImageService = new PartImageService(this);
    m_rebrickableApiClient = new RebrickableApiClient(this);

    initializeUi();

    connect(m_partImageService,
            &PartImageService::imageReady,
            this,
            &InteractiveBuildPullingDialog::setPartImage);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partImageUrlsFinished,
            this,
            [this](const RebrickableApiClient::PartImageUrlsResult& result) {
                if (!result.success)
                    return;

                for (const auto& part : result.parts) {
                    if (part.partImageUrl.trimmed().isEmpty())
                        continue;

                    m_partImageService->requestPartImage(part.partNumber,
                                                         part.partImageUrl);
                }
            });

    refreshView();
}

void InteractiveBuildPullingDialog::initializeUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* summaryLayout = new QHBoxLayout();

    m_buildLabel = new QLabel(this);
    m_progressLabel = new QLabel(this);
    m_remainingLabel = new QLabel(this);
    m_locationsLabel = new QLabel(this);

    QFont summaryFont = m_buildLabel->font();
    summaryFont.setBold(true);

    m_buildLabel->setFont(summaryFont);
    m_progressLabel->setFont(summaryFont);
    m_remainingLabel->setFont(summaryFont);
    m_locationsLabel->setFont(summaryFont);

    summaryLayout->addWidget(m_buildLabel, 2);
    summaryLayout->addWidget(m_progressLabel);
    summaryLayout->addWidget(m_remainingLabel);
    summaryLayout->addWidget(m_locationsLabel);
    summaryLayout->addStretch(1);

    mainLayout->addLayout(summaryLayout);

    m_instructionLabel = new QLabel(
        tr("Enter the quantity physically pulled from each storage location in the Pulled column, "
           "then choose Record Pulls. Rows are ordered by storage location."),
        this);
    m_instructionLabel->setWordWrap(true);
    mainLayout->addWidget(m_instructionLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels(QStringList()
                                        << tr("Image")
                                        << tr("Part")
                                        << tr("Description")
                                        << tr("Color")
                                        << tr("Required")
                                        << tr("Already Pulled")
                                        << tr("Allocated Here")
                                        << tr("Pulled")
                                        << tr("Storage Location"));

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setIconSize(QSize(ImageSize, ImageSize));

    m_table->horizontalHeader()->setSectionResizeMode(ImageColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(PartColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(DescriptionColumn,
                                                       QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColorColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(RequiredColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(AlreadyPulledColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(AllocatedHereColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(PulledColumn,
                                                       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(StorageColumn,
                                                       QHeaderView::Stretch);

    mainLayout->addWidget(m_table, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    auto* buttonLayout = new QHBoxLayout();
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_recordButton = new QPushButton(tr("Record Pulls"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_recordButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_refreshButton,
            &QPushButton::clicked,
            this,
            &InteractiveBuildPullingDialog::refreshView);

    connect(m_recordButton,
            &QPushButton::clicked,
            this,
            &InteractiveBuildPullingDialog::recordPulls);

    connect(m_closeButton,
            &QPushButton::clicked,
            this,
            &QDialog::accept);
}

void InteractiveBuildPullingDialog::refreshView()
{
    const BuildPullingService::PullingView view =
        m_pullingService.getPullingView(m_buildId);

    if (!view.success) {
        m_view = view;
        m_table->setRowCount(0);
        m_rowEditors.clear();
        m_rowsByPartNumber.clear();
        m_buildLabel->setText(tr("Build: unavailable"));
        m_progressLabel->clear();
        m_remainingLabel->clear();
        m_locationsLabel->clear();
        m_statusLabel->setText(view.message);
        updateRecordButton();
        return;
    }

    m_view = view;
    updateHeader(view.summary);
    populateTable(view);

    if (view.items.isEmpty()) {
        if (view.summary.remaining == 0) {
            m_statusLabel->setText(tr("This Build is fully pulled."));
        } else {
            m_statusLabel->setText(
                tr("No allocated stock remains to pull. Use Allocate Available on the Build screen "
                   "after additional inventory is received or allocated."));
        }
    } else {
        m_statusLabel->setText(
            tr("Ready. Enter quantities in the Pulled column and choose Record Pulls."));
    }

    updateRecordButton();
}

void InteractiveBuildPullingDialog::updateHeader(
    const BuildPullingService::PullingSummary& summary)
{
    QString buildText = summary.buildName.trimmed();
    if (!summary.setNumber.trimmed().isEmpty()) {
        if (!buildText.isEmpty())
            buildText += QStringLiteral(" — ");
        buildText += summary.setNumber.trimmed();
    }

    if (buildText.isEmpty())
        buildText = tr("Build %1").arg(summary.buildId);

    m_buildLabel->setText(tr("Build: %1").arg(buildText));
    m_progressLabel->setText(
        tr("Progress: %1 / %2").arg(summary.totalPulled).arg(summary.totalRequired));
    m_remainingLabel->setText(tr("Remaining: %1").arg(summary.remaining));
    m_locationsLabel->setText(
        tr("Locations Remaining: %1").arg(summary.locationsRemaining));
}

void InteractiveBuildPullingDialog::populateTable(
    const BuildPullingService::PullingView& view)
{
    m_table->setUpdatesEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(view.items.size());
    m_rowEditors.clear();
    m_rowsByPartNumber.clear();

    QStringList missingImages;
    QString previousStoragePath;

    for (int row = 0; row < view.items.size(); ++row) {
        const BuildPullingService::PullingItem& item = view.items.at(row);

        m_table->setRowHeight(row, ImageSize + 10);

        auto* imageItem = new QTableWidgetItem();
        imageItem->setTextAlignment(Qt::AlignCenter);
        imageItem->setData(Qt::UserRole, item.partNumber);
        imageItem->setToolTip(item.partNumber);
        m_table->setItem(row, ImageColumn, imageItem);

        auto* partItem = new QTableWidgetItem(item.partNumber);
        auto* descriptionItem = new QTableWidgetItem(item.partName);
        auto* colorItem = new QTableWidgetItem(item.colorName);
        auto* requiredItem = new QTableWidgetItem(QString::number(item.quantityRequired));
        auto* alreadyPulledItem =
            new QTableWidgetItem(QString::number(item.quantityPulledForRequirement));
        auto* allocatedItem =
            new QTableWidgetItem(QString::number(item.quantityAllocatedHere));
        auto* storageItem = new QTableWidgetItem(item.storagePath);

        requiredItem->setTextAlignment(Qt::AlignCenter);
        alreadyPulledItem->setTextAlignment(Qt::AlignCenter);
        allocatedItem->setTextAlignment(Qt::AlignCenter);

        if (item.isSubstitution) {
            QString substitutionText = tr("Substitution for original requirement");
            if (!item.originalPartNumber.trimmed().isEmpty())
                substitutionText += tr(": %1").arg(item.originalPartNumber);
            if (!item.originalColorName.trimmed().isEmpty())
                substitutionText += tr(" / %1").arg(item.originalColorName);

            partItem->setToolTip(substitutionText);
            descriptionItem->setToolTip(substitutionText);
            colorItem->setToolTip(substitutionText);
        }

        if (row == 0 || item.storagePath.compare(previousStoragePath,
                                                  Qt::CaseInsensitive) != 0) {
            QFont locationFont = storageItem->font();
            locationFont.setBold(true);
            storageItem->setFont(locationFont);
        }
        previousStoragePath = item.storagePath;

        m_table->setItem(row, PartColumn, partItem);
        m_table->setItem(row, DescriptionColumn, descriptionItem);
        m_table->setItem(row, ColorColumn, colorItem);
        m_table->setItem(row, RequiredColumn, requiredItem);
        m_table->setItem(row, AlreadyPulledColumn, alreadyPulledItem);
        m_table->setItem(row, AllocatedHereColumn, allocatedItem);
        m_table->setItem(row, StorageColumn, storageItem);

        const int requirementRemaining =
            qMax(item.quantityRequired - item.quantityPulledForRequirement, 0);
        const int maximumPull = qMin(item.quantityAllocatedHere, requirementRemaining);

        auto* pulledSpin = new QSpinBox(m_table);
        pulledSpin->setRange(0, maximumPull);
        pulledSpin->setValue(0);
        pulledSpin->setAlignment(Qt::AlignCenter);
        pulledSpin->setKeyboardTracking(false);
        pulledSpin->setToolTip(
            tr("Enter how many pieces you physically pulled from this storage location."));
        m_table->setCellWidget(row, PulledColumn, pulledSpin);

        RowEditor editor;
        editor.row = row;
        editor.allocationId = item.allocationId;
        editor.buildRequirementId = item.buildRequirementId;
        editor.maximumPull = maximumPull;
        editor.spinBox = pulledSpin;
        m_rowEditors.append(editor);

        connect(pulledSpin,
                qOverload<int>(&QSpinBox::valueChanged),
                this,
                [this](int) {
                    updateRecordButton();
                });

        const QString normalized = normalizedPartNumber(item.partNumber);
        m_rowsByPartNumber[normalized].append(row);

        const QString cachedPath = m_partImageService->cachedImagePath(item.partNumber);
        if (!cachedPath.isEmpty()) {
            setPartImage(item.partNumber, cachedPath);
        } else {
            imageItem->setText(tr("Image"));
            missingImages.append(item.partNumber);
        }
    }

    m_table->setUpdatesEnabled(true);
    requestMissingImages(missingImages);
}

void InteractiveBuildPullingDialog::recordPulls()
{
    QList<BuildPullingService::PullRequest> requests;
    QHash<int, int> pullByRequirement;
    QHash<int, int> remainingByRequirement;

    for (const BuildPullingService::PullingItem& item : m_view.items) {
        const int remaining = qMax(item.quantityRequired - item.quantityPulledForRequirement, 0);
        if (!remainingByRequirement.contains(item.buildRequirementId))
            remainingByRequirement.insert(item.buildRequirementId, remaining);
    }

    for (const RowEditor& editor : m_rowEditors) {
        if (!editor.spinBox)
            continue;

        const int quantity = editor.spinBox->value();
        if (quantity <= 0)
            continue;

        if (quantity > editor.maximumPull) {
            QMessageBox::warning(this,
                                 tr("Interactive Build Pulling"),
                                 tr("One of the entered quantities exceeds the available allocation."));
            return;
        }

        pullByRequirement[editor.buildRequirementId] += quantity;

        BuildPullingService::PullRequest request;
        request.allocationId = editor.allocationId;
        request.quantity = quantity;
        requests.append(request);
    }

    if (requests.isEmpty()) {
        QMessageBox::information(this,
                                 tr("Interactive Build Pulling"),
                                 tr("Enter at least one pulled quantity greater than zero."));
        return;
    }

    for (auto it = pullByRequirement.cbegin(); it != pullByRequirement.cend(); ++it) {
        if (it.value() > remainingByRequirement.value(it.key(), 0)) {
            QMessageBox::warning(
                this,
                tr("Interactive Build Pulling"),
                tr("The entered quantities for one requirement exceed the remaining required quantity. "
                   "Reduce the Pulled values and try again."));
            return;
        }
    }

    const BuildPullingService::PullResult result = m_pullingService.recordPulls(requests);

    if (!result.success) {
        QMessageBox::critical(this,
                              tr("Interactive Build Pulling"),
                              result.message);
        refreshView();
        return;
    }

    const QString successMessage =
        tr("Recorded %1 piece(s) from %2 storage row(s).")
            .arg(result.piecesPulled)
            .arg(result.rowsPulled);

    refreshView();
    m_statusLabel->setText(successMessage);
}

void InteractiveBuildPullingDialog::setPartImage(const QString& partNumber,
                                                   const QString& imagePath)
{
    QPixmap pixmap(imagePath);
    if (pixmap.isNull())
        return;

    const QIcon icon(pixmap.scaled(ImageSize,
                                   ImageSize,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation));

    const QList<int> rows = m_rowsByPartNumber.value(normalizedPartNumber(partNumber));
    for (int row : rows) {
        QTableWidgetItem* item = m_table->item(row, ImageColumn);
        if (!item)
            continue;

        item->setText(QString());
        item->setIcon(icon);
    }
}

void InteractiveBuildPullingDialog::requestMissingImages(const QStringList& partNumbers)
{
    if (partNumbers.isEmpty())
        return;

    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();
    if (apiKey.isEmpty() || RebrickableApiClient::isSessionBlocked())
        return;

    QStringList unique = partNumbers;
    unique.removeDuplicates();

    for (int first = 0; first < unique.size(); first += ImageBatchSize) {
        m_rebrickableApiClient->getPartImageUrls(
            unique.mid(first, ImageBatchSize),
            apiKey,
            RebrickableApiClient::RequestPriority::Background);
    }
}

void InteractiveBuildPullingDialog::updateRecordButton()
{
    bool hasPull = false;

    for (const RowEditor& editor : m_rowEditors) {
        if (editor.spinBox && editor.spinBox->value() > 0) {
            hasPull = true;
            break;
        }
    }

    m_recordButton->setEnabled(m_view.success && hasPull);
}

QString InteractiveBuildPullingDialog::normalizedPartNumber(const QString& partNumber) const
{
    return partNumber.trimmed().toLower();
}
