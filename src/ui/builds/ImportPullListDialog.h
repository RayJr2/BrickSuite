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

#include <QDialog>
#include <QList>

class QLabel;
class QPushButton;
class QTableWidget;

class ImportPullListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportPullListDialog(int buildId, const QString& fileName, QWidget* parent = nullptr);

private:
    enum class RowStatus {
        Pending,
        ExactPull,
        PartialPull,
        ZeroPull,
        NotEntered,
        OverPull,
        AllocationNotFound,
        InvalidRow
    };

    struct PreviewRow
    {
        int allocationIdCsv = 0;

        QString buildName;
        QString setNumber;
        QString partNumber;
        QString partName;
        QString colorName;
        QString storagePath;

        int quantityAllocatedCsv = 0;
        int quantityPulled = 0;

        bool pulledEntered = false;

        int allocationId = 0;
        int inventoryRecordId = 0;

        int partId = 0;
        int colorId = 0;
        int storageLocationId = 0;

        int actualAllocated = 0;

        RowStatus status = RowStatus::Pending;

        QString message;
    };

    bool loadCsv();

    bool parseCsvLine(const QString& line, QStringList& fields) const;

    void buildPreview();

    void reconcile();

    QString statusText(RowStatus status) const;

    QString storagePath(int storageLocationId) const;

    int m_buildId = 0;
    QString m_fileName;

    QList<PreviewRow> m_rows;

    QLabel* m_summaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QTableWidget* m_table = nullptr;

    QPushButton* m_reconcileButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    int m_exactCount = 0;
    int m_partialCount = 0;
    int m_zeroCount = 0;
    int m_problemCount = 0;
};