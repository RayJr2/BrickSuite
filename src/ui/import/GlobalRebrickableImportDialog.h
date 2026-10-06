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

#include "../../import/global/RebrickableImportTypes.h"
#include "../../import/global/RebrickableImportCancellation.h"

#include <QDialog>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QThread;
class QCloseEvent;

class GlobalRebrickableImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GlobalRebrickableImportDialog(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void catalogDataChanged(bool partsChanged, bool setsChanged, bool minifigsChanged,
                            bool buildabilityChanged);

private:
    void selectFolder();
    void rescan();
    void displayPlan(const RebrickableImportPlan& plan);
    void startImport();
    void cancelImport();
    void setRunning(bool running);

    QLineEdit* m_directoryEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_progressLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_rescanButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    bool m_scanRunning = false;
    bool m_importRunning = false;
    RebrickableImportPlan m_plan;
    QThread* m_worker = nullptr;
    std::unique_ptr<RebrickableImportCancellation> m_cancellation;
};
