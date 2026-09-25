#pragma once

#include <QDialog>
#include <memory>

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
    explicit PrintCapabilityAuditDialog(QWidget* parent = nullptr);
protected:
    void reject() override;
private:
    void start();
    void stop();
    QComboBox* m_mode = nullptr;
    QSpinBox* m_sampleCount = nullptr;
    QSpinBox* m_seed = nullptr;
    QPlainTextEdit* m_partList = nullptr;
    QCheckBox* m_excludeNonstandardIds = nullptr;
    QLineEdit* m_output = nullptr;
    QPushButton* m_browseOutput = nullptr;
    QLabel* m_counters = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_stop = nullptr;
    bool m_running = false;
    std::shared_ptr<PrintGeometry::CancellationState> m_cancellation;
};
