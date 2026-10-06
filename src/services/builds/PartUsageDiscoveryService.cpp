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

#include "PartUsageDiscoveryService.h"
#include "../../repositories/PartUsageDiscoveryRepository.h"

#include <QCoreApplication>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>
#include <atomic>

class PartUsageDiscoveryWorker : public QObject
{
public:
    explicit PartUsageDiscoveryWorker(QString path) : m_path(std::move(path)) {}
    PartUsageSearchResult run(const PartUsageSearch& request)
    {
        if (!m_database.isValid()) {
            m_name = QStringLiteral("part-usage-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
            m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
            m_database.setDatabaseName(m_path);
            m_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
            if (!m_database.open()) return {false, QStringLiteral("Unable to open the local catalog database.")};
            QSqlQuery query(m_database);
            query.exec(QStringLiteral("PRAGMA query_only=ON"));
            query.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
        }
        return PartUsageDiscoveryRepository(m_database).search(request);
    }
    void close()
    {
        if (!m_database.isValid()) return;
        m_database.close();
        m_database = {};
        QSqlDatabase::removeDatabase(m_name);
    }
private:
    QString m_path;
    QString m_name;
    QSqlDatabase m_database;
};

PartUsageDiscoveryService::PartUsageDiscoveryService(const QString& path, QObject* parent)
    : QObject(parent), m_generation(std::make_shared<std::atomic<quint64>>(0))
{
    m_thread = new QThread(this);
    m_worker = new PartUsageDiscoveryWorker(path);
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
}

PartUsageDiscoveryService::~PartUsageDiscoveryService()
{
    ++*m_generation;
    if (m_thread && m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_worker, [worker=m_worker] { worker->close(); }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
}

quint64 PartUsageDiscoveryService::search(const PartUsageSearch& request, QObject* context,
                                           Completion completion)
{
    const quint64 generation = ++*m_generation;
    const auto current = m_generation;
    QPointer<QObject> safeContext(context);
    QPointer<PartUsageDiscoveryService> self(this);
    QMetaObject::invokeMethod(m_worker, [=] {
        if (current->load() != generation) return;
        const auto result = m_worker->run(request);
        if (current->load() != generation) return;
        QMetaObject::invokeMethod(qApp, [=] {
            if (self && safeContext && current->load() == generation)
                completion(generation, result);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    return generation;
}

void PartUsageDiscoveryService::cancel() { ++*m_generation; }
