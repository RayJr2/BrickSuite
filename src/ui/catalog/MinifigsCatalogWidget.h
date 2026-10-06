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

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QComboBox;
class MinifigImageService;
class WorkspaceContext;
class RemoteReadApplicationServices;
class RemoteCollectionMutationApplicationService;

class MinifigsCatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MinifigsCatalogWidget(WorkspaceContext& workspaceContext,
                                   QWidget* parent = nullptr,
                                   RemoteReadApplicationServices* remoteReads = nullptr,
                                   RemoteCollectionMutationApplicationService* remoteMutations = nullptr);
    void refresh();

signals:
    void createBuildRequested(int minifigCatalogId, const QString& buildName);
    void collectionItemCreated(int collectionItemId);

private slots:
    void searchMinifigs(const QString& loadingMessage = QString());
    void previousPage();
    void nextPage();
    void importMinifigs();
    void importThemes();
    void displayImage(const QString& minifigNumber, const QString& imagePath);

private:
    void updatePagingControls();
    void loadThemes();
    WorkspaceContext& m_workspaceContext;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteCollectionMutationApplicationService* m_remoteMutations = nullptr;

    int m_currentPage = 0;
    int m_totalResultCount = 0;
    QString m_loadedSearchText;
    int m_loadedThemeCatalogId = 0;
    bool m_refreshInProgress = false;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_importThemesButton = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QLabel* m_resultLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_resultsTable = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;
    MinifigImageService* m_imageService = nullptr;
};
