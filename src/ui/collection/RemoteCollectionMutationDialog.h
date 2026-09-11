#pragma once

#include "../../services/application/dto/RemoteCollectionMutationDtos.h"
#include "../../services/application/dto/RemoteReadDtos.h"

#include <QDialog>
#include <optional>

class RemoteCollectionMutationApplicationService;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTextEdit;

class RemoteCollectionMutationDialog : public QDialog
{
    Q_OBJECT
public:
    RemoteCollectionMutationDialog(const QString& operation, int workspaceId,
        RemoteCollectionMutationApplicationService& service,
        const QList<RemoteReadDto::StorageSummary>& storage,
        const RemoteCollectionMutationDto::Request& seed,
        const QString& reference, const QString& title, QWidget* parent = nullptr);

signals:
    void mutationCompleted(int collectionItemId);
    void refreshRequired();

private:
    void submit();
    void setPending(bool pending);
    RemoteCollectionMutationDto::Request request() const;

    QString m_operation;
    int m_workspaceId = 0;
    RemoteCollectionMutationApplicationService& m_service;
    RemoteCollectionMutationDto::Request m_seed;
    QString m_mutationId;
    bool m_pending = false;
    QComboBox* m_state = nullptr;
    QComboBox* m_condition = nullptr;
    QComboBox* m_completeness = nullptr;
    QComboBox* m_storage = nullptr;
    QLineEdit* m_nickname = nullptr;
    QTextEdit* m_notes = nullptr;
    QLabel* m_status = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};
