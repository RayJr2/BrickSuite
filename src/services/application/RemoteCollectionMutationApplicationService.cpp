#include "RemoteCollectionMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"
RemoteCollectionMutationApplicationService::RemoteCollectionMutationApplicationService(RemoteMutationApplicationServices&m,QObject*p):QObject(p),m_mutations(m){}
bool RemoteCollectionMutationApplicationService::isAvailableFor(const QString&o)const{return m_mutations.isAvailableFor(o,o);}
QString RemoteCollectionMutationApplicationService::submit(const QString&o,const RemoteCollectionMutationDto::Request&r,QObject*c,Completion done,Failure failed){auto fallback=failed;return m_mutations.submit(o,o,RemoteCollectionMutationDto::toMetadata(o,r),c,[done=std::move(done),failed=fallback](const RemoteMutationDto::Result&s){RemoteCollectionMutationDto::Result x;RemoteMutationDto::Error e;if(!RemoteCollectionMutationDto::resultFromMutation(s,&x,&e)){e.outcome=RemoteMutationDto::Outcome::DefinitiveFailure;e.mutationId=s.mutationId;if(failed)failed(e);}else if(done)done(x);},std::move(failed));}
