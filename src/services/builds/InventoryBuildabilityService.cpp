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

#include "InventoryBuildabilityService.h"
#include "../../repositories/InventoryBuildabilityRepository.h"
#include <QCoreApplication>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

class InventoryBuildabilityWorker : public QObject
{
public:
    explicit InventoryBuildabilityWorker(QString path):m_path(std::move(path)){}
    InventoryBuildabilitySearchResult run(const InventoryBuildabilitySearch&r){if(!m_db.isValid()){m_name="buildability-"+QUuid::createUuid().toString(QUuid::WithoutBraces);m_db=QSqlDatabase::addDatabase("QSQLITE",m_name);m_db.setDatabaseName(m_path);m_db.setConnectOptions("QSQLITE_OPEN_READONLY");if(!m_db.open())return{false,"Unable to open the local database."};QSqlQuery q(m_db);q.exec("PRAGMA query_only=ON");q.exec("PRAGMA foreign_keys=ON");}return InventoryBuildabilityRepository(m_db).search(r);}
    void close(){if(!m_db.isValid())return;m_db.close();m_db={};QSqlDatabase::removeDatabase(m_name);}
private:QString m_path,m_name;QSqlDatabase m_db;
};

InventoryBuildabilityService::InventoryBuildabilityService(const QString&p,QObject*parent):QObject(parent),m_generation(std::make_shared<std::atomic<quint64>>(0)){m_thread=new QThread(this);m_worker=new InventoryBuildabilityWorker(p);m_worker->moveToThread(m_thread);connect(m_thread,&QThread::finished,m_worker,&QObject::deleteLater);m_thread->start();}
InventoryBuildabilityService::~InventoryBuildabilityService(){++*m_generation;if(m_thread&&m_thread->isRunning()){QMetaObject::invokeMethod(m_worker,[w=m_worker]{w->close();},Qt::BlockingQueuedConnection);m_thread->quit();m_thread->wait();}}
quint64 InventoryBuildabilityService::search(const InventoryBuildabilitySearch&r,QObject*context,Completion done){const quint64 g=++*m_generation;auto current=m_generation;QPointer<QObject>safe(context);QPointer<InventoryBuildabilityService>self(this);QMetaObject::invokeMethod(m_worker,[=]{if(current->load()!=g)return;auto result=m_worker->run(r);if(current->load()!=g)return;QMetaObject::invokeMethod(qApp,[=]{if(self&&safe&&current->load()==g)done(g,result);},Qt::QueuedConnection);},Qt::QueuedConnection);return g;}
void InventoryBuildabilityService::cancel(){++*m_generation;}
