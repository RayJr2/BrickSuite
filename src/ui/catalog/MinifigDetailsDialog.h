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

class QLabel;
class MinifigImageService;
class PartImageService;
class RebrickableMinifigPartsService;
class QPushButton;
class QTableWidget;
class WorkspaceContext;
class RemoteReadApplicationServices;
class RemoteCollectionMutationApplicationService;

class MinifigDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MinifigDetailsDialog(int minifigCatalogId, WorkspaceContext& workspaceContext,
                                  QWidget* parent = nullptr,
                                  RemoteReadApplicationServices* remoteReads = nullptr,
                                  RemoteCollectionMutationApplicationService* remoteMutations = nullptr);

signals:
    void createBuildRequested(int minifigCatalogId, const QString& buildName);
    void collectionItemCreated(int collectionItemId);

private:
    bool loadMinifig();
    void loadComposition();
    void importPartsList();
    void getPartsFromRebrickable();
    void setCompositionActionsEnabled(bool enabled);
    void createBuildFromStock();
    void addToCollection();
    void displayImage(const QString& imagePath);

    int m_minifigCatalogId = 0;
    WorkspaceContext& m_workspaceContext;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteCollectionMutationApplicationService* m_remoteMutations = nullptr;
    QString m_minifigNumber;
    QString m_minifigName;
    QString m_imageUrl;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_imageStatusLabel = nullptr;
    QLabel* m_numberLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_partsLabel = nullptr;
    QLabel* m_providerLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_compositionSummaryLabel = nullptr;
    QLabel* m_compositionSourceLabel = nullptr;
    QTableWidget* m_compositionTable = nullptr;
    QPushButton* m_importPartsButton = nullptr;
    QPushButton* m_getPartsButton = nullptr;
    QPushButton* m_createBuildButton = nullptr;
    QPushButton* m_addToCollectionButton = nullptr;
    int m_requiredPieces = 0;
    int m_sparePieces = 0;
    MinifigImageService* m_imageService = nullptr;
    PartImageService* m_partImageService = nullptr;
    RebrickableMinifigPartsService* m_rebrickablePartsService = nullptr;
};
