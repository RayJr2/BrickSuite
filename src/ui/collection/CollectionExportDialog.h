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
#include "../../models/export/CollectionExportTypes.h"
#include <QDialog>
class QLabel;class QListWidget;class QPushButton;class QTableWidget;
class CollectionExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CollectionExportDialog(QList<CollectionExportRow>,QString,QWidget* parent=nullptr);
private:
    void apply(const CollectionExportConfiguration&);
    CollectionExportConfiguration configuration() const;
    void updatePreview();
    void moveField(int);
    void exportCsv();
    QList<CollectionExportRow> m_rows;
    QString m_filterSummary;
    QListWidget* m_fields=nullptr;
    QTableWidget* m_preview=nullptr;
    QLabel* m_status=nullptr;
    QPushButton* m_export=nullptr;
};
