#include "RemoteBuildMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"

RemoteBuildMutationApplicationService::RemoteBuildMutationApplicationService(
    RemoteMutationApplicationServices& mutations,QObject* parent)
    : QObject(parent),m_mutations(mutations) {}

bool RemoteBuildMutationApplicationService::isAvailableFor(const QString& operation) const
{ return m_mutations.isAvailableFor(operation,operation); }

QString RemoteBuildMutationApplicationService::submit(
    const QString& operation,const RemoteBuildMutationDto::Request& request,QObject* context,
    Completion completion,Failure failure)
{
    const auto fallback=failure;
    return m_mutations.submit(operation,operation,
        RemoteBuildMutationDto::toMetadata(operation,request),context,
        [completion=std::move(completion),failure=fallback](const RemoteMutationDto::Result& source){
            RemoteBuildMutationDto::Result result;RemoteMutationDto::Error error;
            if(!RemoteBuildMutationDto::resultFromMutation(source,&result,&error)){
                error.outcome=RemoteMutationDto::Outcome::Unknown;error.mutationId=source.mutationId;
                if(failure)failure(error);
            }else if(completion)completion(result);
        },std::move(failure));
}
