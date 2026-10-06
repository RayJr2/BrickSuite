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

#include "../../services/inventory/InventoryColorAuditCsv.h"

#include <QDialog>

class WorkspaceContext;
class QLabel;
class QLineEdit;
class QPushButton;

class InventoryColorAuditReviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit InventoryColorAuditReviewDialog(const QString& fileName,
                                             WorkspaceContext& workspaceContext,
                                             QWidget* parent = nullptr);
    bool isReady() const { return m_ready; }

signals:
    void statusMessageRequested(const QString& message);

private:
    void showRow(int rowIndex);
    void refreshAuthoritativeState();
    void navigate(int delta);
    void dispose(const QString& status);
    void openEditor();
    void updateStatistics();
    QString field(const QString& name) const;
    void setValue(QLabel* label, const QString& value);

    InventoryColorAuditCsv m_csv;
    WorkspaceContext& m_workspaceContext;
    QVector<int> m_reviewRows;
    int m_rowIndex = -1;
    int m_position = -1;
    bool m_recordExists = false;
    bool m_ready = false;

    QLabel* m_progress = nullptr;
    QLabel* m_statistics = nullptr;
    QLabel* m_part = nullptr;
    QLabel* m_description = nullptr;
    QLabel* m_storedColor = nullptr;
    QLabel* m_knownColors = nullptr;
    QLabel* m_suggested = nullptr;
    QLabel* m_quantity = nullptr;
    QLabel* m_storage = nullptr;
    QLabel* m_workspace = nullptr;
    QLabel* m_confidence = nullptr;
    QLabel* m_reason = nullptr;
    QLabel* m_recordId = nullptr;
    QLabel* m_currentState = nullptr;
    QLineEdit* m_note = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_notFound = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
};
