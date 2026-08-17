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

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class SetsCatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SetsCatalogWidget(QWidget* parent = nullptr);

    void refresh();

private slots:
    void searchSets();
    void previousPage();
    void nextPage();
    void importSetsCsv();

signals:
    void createBuildRequested(int setCatalogId, const QString& inventoryMode);

private:
    void loadYears();
    void updatePagingControls();

    int m_currentPage = 0;
    int m_lastResultCount = 0;
    int m_totalResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;

    QComboBox* m_yearCombo = nullptr;

    QPushButton* m_searchButton = nullptr;
    QPushButton* m_importButton = nullptr;

    QLabel* m_resultLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;

    QTableWidget* m_resultsTable = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;
};