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
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;
class PartImageService;
class RebrickableApiClient;

class PartsCatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PartsCatalogWidget(QWidget* parent = nullptr);
    void settingsChanged();

private slots:
    void searchParts();
    void previousPage();
    void nextPage();
    void importPartsCsv();

signals:
    void addPartToInventoryRequested(int partId);

private:
    void loadCategories();
    void updatePagingControls();
    void requestMissingPartImages(const QStringList& partNumbers);

    static constexpr int ResultsPerPage = 250;
    static constexpr int PartImageBatchSize = 20;

    int m_currentPage = 0;
    int m_lastResultCount = 0;
    int m_totalResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QPushButton* m_searchButton = nullptr;

    QTableWidget* m_resultsTable = nullptr;
    QLabel* m_resultLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QHash<QString, int> m_rowByPartNumber;

    QPushButton* m_importPartsButton = nullptr;
};