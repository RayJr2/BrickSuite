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

#include "HelpTopic.h"

#include <QDialog>

class QCloseEvent;
class QLineEdit;
class QPushButton;
class QSplitter;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget* parent = nullptr);

    void showTopic(HelpTopic topic);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildContents();
    void applySearch(const QString& searchText);
    void loadTopic(HelpTopic topic);
    void updateNavigationButtons();
    void syncContentsSelection(const QUrl& source);

    QString searchableText(HelpTopic topic) const;

    QLineEdit* m_searchEdit = nullptr;
    QTreeWidget* m_contentsTree = nullptr;
    QTextBrowser* m_browser = nullptr;
    QSplitter* m_splitter = nullptr;

    QPushButton* m_backButton = nullptr;
    QPushButton* m_forwardButton = nullptr;
    QPushButton* m_homeButton = nullptr;

    bool m_syncingSelection = false;
};
