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

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class InventoryColorAuditCsv
{
public:
    struct Row {
        QStringList values;
    };
    struct Statistics {
        int total = 0;
        int pending = 0;
        int fixed = 0;
        int verifiedOk = 0;
        int skipped = 0;
        int notFound = 0;
    };

    bool load(const QString& fileName, QString* errorMessage = nullptr);
    bool saveDisposition(int rowIndex, const QString& status, const QString& note,
                         QString* errorMessage = nullptr);

    QString fileName() const { return m_fileName; }
    int rowCount() const { return m_rows.size(); }
    const QStringList& headers() const { return m_headers; }
    QString value(int rowIndex, const QString& column) const;
    int firstPendingRow() const;
    int nextPendingRow(int afterRow) const;
    QVector<int> reviewRows(bool skippedOnly = false) const;
    Statistics statistics() const;
    static QString originalBackupPath(const QString& fileName);

private:
    bool write(QString* errorMessage);
    static bool parse(const QByteArray& data, QVector<QStringList>& records, QString* errorMessage);
    static QByteArray encode(const QStringList& headers, const QVector<Row>& rows);
    int column(const QString& name) const;

    QString m_fileName;
    QStringList m_headers;
    QVector<Row> m_rows;
    QHash<QString, int> m_columns;
    qint64 m_loadedSize = -1;
    QDateTime m_loadedModifiedUtc;
    bool m_needsOriginalBackup = false;
};
