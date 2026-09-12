#pragma once
#include "../../services/application/dto/RemoteReadDtos.h"
#include "../../services/application/dto/RemoteInventoryMutationDtos.h"
#include <QDialog>
#include <QHash>
#include <QStringList>
#include <optional>
class RemoteInventoryMutationApplicationService;
class QComboBox; class QLineEdit; class QSpinBox; class QDialogButtonBox; class QLabel; class QCheckBox;
class RemoteInventoryMutationDialog : public QDialog
{
public:
    RemoteInventoryMutationDialog(const QString&,int,RemoteInventoryMutationApplicationService&,
        const QHash<int,QString>&,const QStringList&,
        std::optional<RemoteReadDto::InventoryDetail>,QWidget* parent=nullptr);
    RemoteInventoryMutationDialog(int,RemoteInventoryMutationApplicationService&,
        const QHash<int,QString>&,const RemoteReadDto::LostInventoryRow&,QWidget* parent=nullptr);
private:
    void initialize(const QHash<int,QString>&); void submit(); void setPending(bool);
    RemoteInventoryMutationDto::Request request() const;
    QString m_operation; int m_workspaceId=0; RemoteInventoryMutationApplicationService& m_service;
    QStringList m_hostManufacturerNames;
    std::optional<RemoteReadDto::InventoryDetail> m_detail; std::optional<RemoteReadDto::LostInventoryRow> m_lost;
    QString m_mutationId; bool m_pending=false; QLabel *m_partLabel=nullptr,*m_contextLabel=nullptr,*m_status=nullptr;
    QLineEdit *m_replacementPart=nullptr,*m_notes=nullptr; QComboBox *m_color=nullptr,*m_manufacturer=nullptr,
        *m_condition=nullptr,*m_ownership=nullptr,*m_storage=nullptr; QSpinBox* m_quantity=nullptr;
    QCheckBox* m_showAllColors=nullptr; QDialogButtonBox* m_buttons=nullptr;
};
