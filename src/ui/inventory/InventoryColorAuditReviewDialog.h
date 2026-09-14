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
