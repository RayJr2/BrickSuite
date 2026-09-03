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
class QPushButton;
class QWidget;
class BricksetService;
class QFormLayout;
class SetImageService;
class SetDetailsProviderService;
class PartImageService;
class RebrickableSetPartsService;
class QTableWidget;
class WorkspaceContext;

class SetDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetDetailsDialog(int setCatalogId, WorkspaceContext& workspaceContext,
                              QWidget* parent = nullptr);

signals:
    void createBuildRequested(int setCatalogId, const QString& buildName);
    void collectionItemCreated(int collectionItemId);

private:
    bool loadSet();
    void loadCachedImage();
    void requestImage();
    void requestProviderEnrichment();
    void requestBricksetInstructions();
    void setBricksetRowsVisible(bool visible);
    void loadComposition();
    void getPartsFromRebrickable();
    void importPartsList();
    void createBuildFromStock();
    void addToCollection();
    void setCompositionActionsEnabled(bool enabled);

    int m_setCatalogId = 0;
    WorkspaceContext& m_workspaceContext;

    QString m_setNumber;
    QString m_setName;
    QString m_imageUrl;

    QLabel* m_imageLabel = nullptr;

    QLabel* m_setNumberLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_yearLabel = nullptr;
    QLabel* m_themeLabel = nullptr;
    QLabel* m_partsLabel = nullptr;
    QLabel* m_imageStatusLabel = nullptr;

    QLabel* m_providerSourceLabel = nullptr;
    QLabel* m_providerStatusLabel = nullptr;
    QLabel* m_bricksetIdLabel = nullptr;
    QLabel* m_providerThemeLabel = nullptr;
    QLabel* m_providerSubthemeLabel = nullptr;
    QLabel* m_minifigsLabel = nullptr;
    QLabel* m_availabilityLabel = nullptr;
    QLabel* m_ratingLabel = nullptr;
    QLabel* m_instructionsLabel = nullptr;
    QWidget* m_instructionsRowWidget = nullptr;
    QPushButton* m_viewInstructionsButton = nullptr;
    QLabel* m_additionalImagesLabel = nullptr;
    QLabel* m_providerLinkLabel = nullptr;
    QFormLayout* m_providerLayout = nullptr;

    QLabel* m_compositionSummaryLabel = nullptr;
    QLabel* m_compositionStatusLabel = nullptr;
    QTableWidget* m_compositionTable = nullptr;
    QPushButton* m_getPartsButton = nullptr;
    QPushButton* m_importPartsButton = nullptr;
    QPushButton* m_createBuildButton = nullptr;
    QPushButton* m_addToCollectionButton = nullptr;
    int m_requiredPieces = 0;
    int m_sparePieces = 0;

    SetImageService* m_imageService = nullptr;
    SetDetailsProviderService* m_providerService = nullptr;
    BricksetService* m_bricksetService = nullptr;
    PartImageService* m_partImageService = nullptr;
    RebrickableSetPartsService* m_rebrickablePartsService = nullptr;
};
