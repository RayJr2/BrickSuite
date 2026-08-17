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

#include "../../services/RebrickableApiClient.h"

#include <QDialog>
#include <QList>

class QLabel;
class QPushButton;
class QTableWidget;

class SetImportPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetImportPreviewDialog(int buildId,
                                    const QString& setNumber,
                                    QWidget* parent = nullptr);

private:
    enum class ImportStatus { New, NoChange, QuantityChanged, MappingProblem };

    struct PreviewRow
    {
        RebrickableApiClient::SetPart setPart;

        int partId = 0;
        int colorId = 0;

        int existingRequirementId = 0;
        int existingQuantity = 0;

        ImportStatus status = ImportStatus::MappingProblem;
    };

private:
    void loadFromRebrickable();

    void handleSetDetails(const RebrickableApiClient::SetDetailsResult& result);

    void handleSetParts(const RebrickableApiClient::SetPartsResult& result);

    void tryPopulatePreview();

    void populatePreview();

    void setLoadingState(bool loading);

    void importRequirements();

    QString importStatusText(ImportStatus status) const;

    void updateImportButtonState();

    int m_buildId = 0;

    QString m_setNumber;

    bool m_setDetailsReceived = false;
    bool m_setPartsReceived = false;

    bool m_setDetailsSucceeded = false;
    bool m_setPartsSucceeded = false;

    RebrickableApiClient::SetDetails m_setDetails;

    RebrickableApiClient::SetPartsResult m_setPartsResult;

    RebrickableApiClient* m_apiClient = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QTableWidget* m_previewTable = nullptr;

    QPushButton* m_closeButton = nullptr;

    QList<PreviewRow> m_previewRows;

    int m_newCount = 0;
    int m_quantityChangedCount = 0;
    int m_noChangeCount = 0;
    int m_problemCount = 0;

    QPushButton* m_importButton = nullptr;
};
