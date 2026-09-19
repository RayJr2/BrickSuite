#include "../src/ui/parts/PrintPreparationCoordinator.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

namespace { bool check(bool value,const char*message){if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);bool ok=true;PrintPreparationCoordinator coordinator;PrintGeometry::PrintPreparationRequest request;request.partReference="invalid";request.ldrawIdentity="invalid";request.libraryAuthority="test";
    QEventLoop loop;quint64 deliveredGeneration=0;PrintGeometry::PrintPreparationResult delivered;
    QObject::connect(&coordinator,&PrintPreparationCoordinator::completed,&loop,[&](quint64 generation,const auto&result){deliveredGeneration=generation;delivered=result;loop.quit();});
    ok&=check(coordinator.start(42,request),"first asynchronous request accepted");ok&=check(coordinator.busy(),"coordinator reports active job");ok&=check(!coordinator.start(43,request),"duplicate active job rejected");
    QTimer::singleShot(5000,&loop,&QEventLoop::quit);loop.exec();
    ok&=check(deliveredGeneration==42,"request generation preserved");ok&=check(delivered.state==PrintGeometry::PrintPreparationState::Failed,"worker result delivered safely");ok&=check(!coordinator.busy(),"active state cleared after completion");
    ok&=check(coordinator.start(44,request),"coordinator reusable after completion");coordinator.cancel();QEventLoop cancelled;QObject::connect(&coordinator,&PrintPreparationCoordinator::completed,&cancelled,[&](quint64,const auto&){cancelled.quit();});QTimer::singleShot(5000,&cancelled,&QEventLoop::quit);cancelled.exec();ok&=check(!coordinator.busy(),"cancelled request cleaned up");
    return ok?0:1;
}
