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
#include "../../services/application/dto/RemoteReadDtos.h"
#include "../../services/application/dto/RemoteInventoryMutationDtos.h"
#include <QDialog>
#include <QHash>
#include <QStringList>
#include <optional>
class RemoteInventoryMutationApplicationService;
class SessionStorageSelectionService;
class QComboBox; class QLineEdit; class QSpinBox; class QDialogButtonBox; class QLabel; class QCheckBox;
class RemoteInventoryMutationDialog : public QDialog
{
public:
    RemoteInventoryMutationDialog(const QString&,int,RemoteInventoryMutationApplicationService&,
        const QHash<int,QString>&,const QStringList&,
        std::optional<RemoteReadDto::InventoryDetail>,
        SessionStorageSelectionService* sessionStorageSelections=nullptr,
        const QString& remoteAuthority={},
        const QList<RemoteReadDto::StorageSummary>& remoteStorage={},
        QWidget* parent=nullptr);
    RemoteInventoryMutationDialog(int,RemoteInventoryMutationApplicationService&,
        const QHash<int,QString>&,const RemoteReadDto::LostInventoryRow&,QWidget* parent=nullptr);
private:
    void initialize(const QHash<int,QString>&); void submit(); void setPending(bool);
    RemoteInventoryMutationDto::Request request() const;
    QString m_operation; int m_workspaceId=0; RemoteInventoryMutationApplicationService& m_service;
    SessionStorageSelectionService* m_sessionStorageSelections=nullptr;
    QString m_remoteAuthority;
    QList<RemoteReadDto::StorageSummary> m_remoteStorage;
    QStringList m_hostManufacturerNames;
    std::optional<RemoteReadDto::InventoryDetail> m_detail; std::optional<RemoteReadDto::LostInventoryRow> m_lost;
    QString m_mutationId; bool m_pending=false; QLabel *m_partLabel=nullptr,*m_contextLabel=nullptr,*m_status=nullptr;
    QLineEdit *m_replacementPart=nullptr,*m_notes=nullptr; QComboBox *m_color=nullptr,*m_manufacturer=nullptr,
        *m_condition=nullptr,*m_ownership=nullptr,*m_storage=nullptr; QSpinBox* m_quantity=nullptr;
    QCheckBox* m_showAllColors=nullptr; QDialogButtonBox* m_buttons=nullptr;
};
