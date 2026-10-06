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
class QCheckBox;

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
    void submitRequest(const QString& operation,
                       const RemoteCollectionMutationDto::Request& value,
                       bool submitPartsSourceAfter);
    RemoteCollectionMutationDto::Request request() const;

    QString m_operation;
    int m_workspaceId = 0;
    RemoteCollectionMutationApplicationService& m_service;
    RemoteCollectionMutationDto::Request m_seed;
    QString m_mutationId;
    std::optional<RemoteCollectionMutationDto::Request> m_retainedRequest;
    QString m_retainedOperation;
    bool m_pending = false;
    QComboBox* m_state = nullptr;
    QComboBox* m_condition = nullptr;
    QComboBox* m_completeness = nullptr;
    QComboBox* m_storage = nullptr;
    QLineEdit* m_nickname = nullptr;
    QTextEdit* m_notes = nullptr;
    QCheckBox* m_allowPartsSource = nullptr;
    QLabel* m_status = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};
