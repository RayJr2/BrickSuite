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

#include <QHash>
#include <QList>
#include <QSet>
#include <QWidget>

class WorkspaceContext;
class SessionStorageSelectionService;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QShowEvent;
class QLabel;
class PartImageService;
class RebrickableApiClient;
class AddInventoryDialog;

class MyInventoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MyInventoryWidget(
        WorkspaceContext& workspaceContext,
        SessionStorageSelectionService& sessionStorageSelectionService,
        QWidget* parent = nullptr);

    void refresh();
    void settingsChanged();
    void reloadManufacturers();

    void updatePartColorImage(const QString& partNumber,
                              int rebrickableColorId,
                              const QString& imagePath);

    bool hasActiveAddInventoryDialog() const;
    void sendPartToActiveAddInventoryDialog(const QString& partNumber);

signals:
    void inventoryChanged();
    void addInventoryDialogAvailabilityChanged(bool available);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void workspaceChanged(int workspaceId);
    void searchInventory();
    void previousPage();
    void nextPage();
    void importCsv();

private:
    void loadCategories();
    void loadColors();
    void loadManufacturers();
    void loadStorageLocations();
    void updatePagingControls();
    void addPart();
    void showLostInventory();

    QString storagePathForId(int storageLocationId) const;

    static constexpr int ResultsPerPage = 250;

    WorkspaceContext& m_workspaceContext;
    SessionStorageSelectionService& m_sessionStorageSelectionService;

    int m_currentPage = 0;
    int m_lastResultCount = 0;
    int m_totalResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_storageCombo = nullptr;
    QComboBox* m_manufacturerCombo = nullptr;

    QPushButton* m_searchButton = nullptr;

    QTableWidget* m_resultsTable = nullptr;
    QLabel* m_resultLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;

    QPushButton* m_importButton = nullptr;

    QPushButton* m_addPartButton = nullptr;

    QHash<int, QString> m_storagePathById;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;
    AddInventoryDialog* m_activeAddInventoryDialog = nullptr;

    QHash<QString, QList<int>> m_rowsByPartNumber;
    QSet<QString> m_partDetailsRequested;

    QString partColorKey(const QString& partNumber, int rebrickableColorId) const;
    QHash<QString, QList<int>> m_rowsByPartColor;
    QSet<int> m_rowsWithColorImage;

    QPushButton* m_lostInventoryButton = nullptr;
};
