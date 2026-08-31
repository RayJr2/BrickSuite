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

#pragma once

#include "../../services/builds/BuildPullingService.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QString>

class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class PartImageService;
class RebrickableApiClient;

class InteractiveBuildPullingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InteractiveBuildPullingDialog(int buildId, QWidget* parent = nullptr);

private slots:
    void recordPulls();
    void refreshView();
    void setPartImage(const QString& partNumber, const QString& imagePath);

private:
    struct RowEditor
    {
        int row = -1;
        int allocationId = 0;
        int buildRequirementId = 0;
        int maximumPull = 0;
        QSpinBox* spinBox = nullptr;
    };

    void initializeUi();
    void populateTable(const BuildPullingService::PullingView& view);
    void updateHeader(const BuildPullingService::PullingSummary& summary);
    void requestMissingImages(const QStringList& partNumbers);
    void updateRecordButton();
    QString normalizedPartNumber(const QString& partNumber) const;

    int m_buildId = 0;

    BuildPullingService m_pullingService;
    BuildPullingService::PullingView m_view;

    QLabel* m_buildLabel = nullptr;
    QLabel* m_progressLabel = nullptr;
    QLabel* m_remainingLabel = nullptr;
    QLabel* m_locationsLabel = nullptr;
    QLabel* m_instructionLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QTableWidget* m_table = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_recordButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QList<RowEditor> m_rowEditors;
    QHash<QString, QList<int>> m_rowsByPartNumber;
};
