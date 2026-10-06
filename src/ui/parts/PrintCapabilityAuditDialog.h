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
#include <memory>
#include "../../services/geometry/print/BatchPrintableModelService.h"
#include <QFuture>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace PrintGeometry { class CancellationState; }

class PrintCapabilityAuditDialog final : public QDialog {
public:
    using Runner = std::function<PrintGeometry::BatchPrintRun(const PrintGeometry::BatchPrintOptions&,
        PrintGeometry::CancellationState*,const PrintGeometry::BatchPrintableModelService::Progress&)>;
    explicit PrintCapabilityAuditDialog(QWidget* parent = nullptr,Runner runner = {});
    ~PrintCapabilityAuditDialog() override;
protected:
    void reject() override;
    void closeEvent(QCloseEvent* event) override;
private:
    void start();
    void stop();
    void showIncompleteRuns();
    void showPhase(int sequence,const QString& part,const QString& phase);
    void refreshCorpus(const QString& savedPlan = {});
    void updateMode();
    QComboBox* m_mode = nullptr;
    QComboBox* m_fitProfile = nullptr;
    QComboBox* m_printOrientation = nullptr;
    bool m_continuationPlan = false;
    QSpinBox* m_sampleCount = nullptr;
    QSpinBox* m_seed = nullptr;
    QSpinBox* m_referenceFirst = nullptr;
    QSpinBox* m_referenceCount = nullptr;
    QLabel* m_referenceCounts = nullptr;
    QPushButton* m_loadPlan = nullptr;
    QPushButton* m_refreshCorpus = nullptr;
    QCheckBox* m_noColor = nullptr;
    PrintGeometry::BatchPrintCorpus m_corpus;
    bool m_discovering = false;
    QFuture<PrintGeometry::BatchPrintCorpus> m_discoveryFuture;
    QPlainTextEdit* m_partList = nullptr;
    QCheckBox* m_excludeNoModel = nullptr;
    QLabel* m_recovery = nullptr;
    QCheckBox* m_excludeNonstandardIds = nullptr;
    QLineEdit* m_output = nullptr;
    QPushButton* m_browseOutput = nullptr;
    QLabel* m_counters = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_stop = nullptr;
    bool m_running = false, m_closePending = false;
    Runner m_runner;
    QFuture<PrintGeometry::BatchPrintRun> m_future;
    std::shared_ptr<PrintGeometry::BatchPrintRunState> m_runState;
    int m_currentSequence = 0;
    QString m_currentPart, m_currentPhase;
    std::shared_ptr<PrintGeometry::CancellationState> m_cancellation;
};
