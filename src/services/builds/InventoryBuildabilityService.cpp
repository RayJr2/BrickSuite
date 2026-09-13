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
