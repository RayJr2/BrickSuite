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
    QComboBox* m_mode = nullptr;
    QSpinBox* m_sampleCount = nullptr;
    QSpinBox* m_seed = nullptr;
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
