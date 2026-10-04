#pragma once

#include <QDialog>
#include "../../services/inventory/CatalogSetPartOutService.h"

class QCheckBox; class QComboBox; class QDialogButtonBox; class QLabel;
class QLineEdit; class QProgressBar; class QSpinBox; class QTableWidget; class QTimer;
template<class T> class QFutureWatcher;

class PartOutSetDialog : public QDialog
{
    Q_OBJECT
public:
    PartOutSetDialog(int setCatalogId, int workspaceId, bool remote, QWidget* parent = nullptr);
    bool isSubmitting() const { return m_submitting; }
signals:
    void inventoryCommitted(int workspaceId, int storageId, bool storageCreated);
protected:
    void done(int result) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    CatalogSetPartOutService::Request request() const;
    void schedulePreview();
    void preview();
    void showPlan(const CatalogSetPartOutService::Plan& plan);
    void submit();
    int m_setId, m_workspaceId;
    bool m_remote = false, m_submitting = false, m_previewQueued = false;
    QString m_databasePath, m_operationId;
    CatalogSetPartOutService::Plan m_plan;
    QWidget* m_fields = nullptr;
    QSpinBox* m_copies = nullptr;
    QComboBox *m_condition = nullptr, *m_mode = nullptr, *m_existing = nullptr, *m_type = nullptr, *m_parent = nullptr;
    QCheckBox* m_spares = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel *m_summary = nullptr, *m_totals = nullptr, *m_status = nullptr;
    QTableWidget* m_table = nullptr;
    QProgressBar* m_progress = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QTimer* m_previewTimer = nullptr;
    QFutureWatcher<CatalogSetPartOutService::Plan>* m_previewWatcher = nullptr;
};
