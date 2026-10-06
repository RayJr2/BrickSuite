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
#include <QList>
#include "../../models/PartReferenceEntry.h"
#include "../../services/parts/PartReferenceManifest.h"

class QComboBox; class QLabel; class QLineEdit; class QListWidget; class QPushButton;
class SharedPartReferenceCustomizationService;
class RemotePartReferenceMutationApplicationService;
class RemoteSessionState;

class AddPartReferenceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddPartReferenceDialog(SharedPartReferenceCustomizationService& customizationService,
                                    int initialPartId = 0,
                                    const PartReferenceEntry* anchor = nullptr,
                                    RemotePartReferenceMutationApplicationService* remoteMutations = nullptr,
                                    RemoteSessionState* remoteSession = nullptr,
                                    QWidget* parent = nullptr);
    bool customizationAdded() const { return m_added; }
private:
    void searchParts();
    void destinationChanged();
    void save();
    int selectedPartId() const;
    PartReferenceManifest m_manifest;
    QList<PartReferenceEntry> m_effective;
    QLineEdit* m_search = nullptr;
    QListWidget* m_results = nullptr;
    QComboBox* m_destination = nullptr;
    QComboBox* m_placement = nullptr;
    QComboBox* m_anchor = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_save = nullptr;
    int m_initialPartId = 0;
    QString m_defaultCatalog;
    QString m_defaultSection;
    QString m_defaultAnchor;
    bool m_added = false;
    bool m_pending = false;
    QString m_mutationId;
    SharedPartReferenceCustomizationService& m_customizationService;
    RemotePartReferenceMutationApplicationService* m_remoteMutations = nullptr;
    RemoteSessionState* m_remoteSession = nullptr;
    quint64 m_sessionGeneration = 0;
    quint64 m_workspaceGeneration = 0;
};
