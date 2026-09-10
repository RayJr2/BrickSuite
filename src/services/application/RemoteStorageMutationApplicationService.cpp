#include "RemoteStorageMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"
#include <QDebug>
RemoteStorageMutationApplicationService::RemoteStorageMutationApplicationService(RemoteMutationApplicationServices&m,QObject*p):QObject(p),m_mutations(m){}
bool RemoteStorageMutationApplicationService::isAvailableFor(const QString&o)const{return m_mutations.isAvailableFor(o,o);}
QString RemoteStorageMutationApplicationService::submit(const QString&o,const RemoteStorageMutationDto::Request&r,QObject*c,Completion done,Failure failed)
{const auto decodeFailure=failed;return m_mutations.submit(o,o,RemoteStorageMutationDto::toMetadata(o,r),c,[done=std::move(done),failed=decodeFailure](const RemoteMutationDto::Result&s){RemoteStorageMutationDto::Result result;RemoteMutationDto::Error error;if(!RemoteStorageMutationDto::resultFromMutation(s,&result,&error)){error.outcome=RemoteMutationDto::Outcome::Unknown;error.mutationId=s.mutationId;if(failed)failed(error);}else if(done)done(result);},std::move(failed));}
#define STORAGE_METHOD(name,op) QString RemoteStorageMutationApplicationService::name(const RemoteStorageMutationDto::Request&r,QObject*c,Completion d,Failure f){return submit(QStringLiteral(op),r,c,std::move(d),std::move(f));}
STORAGE_METHOD(add,"storage.add") STORAGE_METHOD(edit,"storage.edit") STORAGE_METHOD(setActive,"storage.setActive")
#undef STORAGE_METHOD
