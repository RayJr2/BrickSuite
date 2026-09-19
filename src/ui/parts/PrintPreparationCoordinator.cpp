#include "PrintPreparationCoordinator.h"

#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QPointer>

PrintPreparationCoordinator::PrintPreparationCoordinator(QObject*parent):QObject(parent),m_cache(std::make_shared<PrintGeometry::PrintPreparationCache>()),m_service(std::make_shared<PrintGeometry::LDrawPrintPreparationService>(m_cache)){}

bool PrintPreparationCoordinator::start(quint64 generation,const PrintGeometry::PrintPreparationRequest&request)
{
    if(m_busy)return false;m_busy=true;emit busyChanged(true);m_cancellation=std::make_shared<PrintGeometry::CancellationState>();
    auto cancellation=m_cancellation;auto*watcher=new QFutureWatcher<PrintGeometry::PrintPreparationResult>(this);QPointer<PrintPreparationCoordinator>self(this);
    auto callback=[self,generation](const PrintGeometry::PrintPreparationProgress&p){if(!self)return;QMetaObject::invokeMethod(self,[self,generation,p]{if(self&&self->m_busy)emit self->progress(generation,p);},Qt::QueuedConnection);};
    connect(watcher,&QFutureWatcher<PrintGeometry::PrintPreparationResult>::finished,this,[this,self,watcher,generation]{auto result=watcher->result();watcher->deleteLater();if(!self)return;m_busy=false;m_cancellation.reset();emit busyChanged(false);emit completed(generation,std::move(result));});
    auto service=m_service;watcher->setFuture(QtConcurrent::run([service,request,cancellation,callback]{return service->prepare(request,cancellation.get(),callback);}));return true;
}

void PrintPreparationCoordinator::cancel(){if(m_cancellation)m_cancellation->cancel();}
