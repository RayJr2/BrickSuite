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
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "../../models/procurement/BrickLinkWantedListOptions.h"
#include "../../models/procurement/ProcurementDraft.h"
#include "../../services/procurement/ProcurementPartEnrichmentSession.h"

#include <QDialog>
#include <QPointer>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class PartExternalIdEnrichmentService;

class ProcurementPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProcurementPreviewDialog(const ProcurementDraft& draft,
                                      PartExternalIdEnrichmentService* enrichmentService,
                                      QWidget* parent = nullptr);

    const ProcurementDraft& draft() const;
    BrickLinkWantedListOptions brickLinkOptions() const;

private:
    struct BrickLinkColorChoice
    {
        QString externalId;
        QString displayName;
    };

    void buildUi();
    void populateRows();
    QList<BrickLinkColorChoice> loadMappedBrickLinkColors() const;

    void updatePartRow(int row);
    void updateColorRow(int row);
    void updateRowStatus(int row);
    void updateSummary();
    void initializeAutomaticEnrichment();
    void submitEnrichment(const QList<int>& partIds);
    void handleEnrichmentFinished(int partId, int outcome);
    void refreshAutomaticResolution(int partId);
    void updateEnrichmentStatus();
    bool persistRememberedPartOverrides();
    void generateBrickLinkXml();

    ProcurementDraft m_draft;
    QPointer<PartExternalIdEnrichmentService> m_enrichmentService;
    ProcurementPartEnrichmentSession m_enrichmentSession;

    QLabel* m_buildLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_enrichmentStatusLabel = nullptr;
    QPushButton* m_retryEnrichmentButton = nullptr;

    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_notifyCombo = nullptr;
    QComboBox* m_wantedShowCombo = nullptr;
    QComboBox* m_remarksCombo = nullptr;
    QLineEdit* m_customRemarksEdit = nullptr;

    QTableWidget* m_table = nullptr;
    QList<QCheckBox*> m_rememberPartOverrideChecks;
    QPushButton* m_generateButton = nullptr;
};
