#include "RemotePullingApplicationService.h"
#include "RemoteMutationApplicationServices.h"

RemotePullingApplicationService::RemotePullingApplicationService(
    RemoteMutationApplicationServices& mutations, QObject* parent)
    : QObject(parent), m_mutations(mutations) {}

bool RemotePullingApplicationService::isAvailable() const
{
    return m_mutations.isAvailableFor(QStringLiteral("builds.pulling.record"),
                                      QStringLiteral("builds.pulling.write"));
}

QString RemotePullingApplicationService::record(
    const RemotePullingMutationDto::Request& request, QObject* context,
    std::function<void(const RemotePullingMutationDto::Result&)> completion,
    std::function<void(const RemoteMutationDto::Error&)> failure)
{
    return m_mutations.submit(QStringLiteral("builds.pulling.record"),
        QStringLiteral("builds.pulling.write"), RemotePullingMutationDto::toMetadata(request), context,
        [completion=std::move(completion), failure](const RemoteMutationDto::Result& source) {
            RemotePullingMutationDto::Result result;
            RemoteMutationDto::Error error;
            if (!RemotePullingMutationDto::resultFromMutation(source, &result, &error)) {
                if (failure) failure(error);
            } else if (completion) completion(result);
        }, std::move(failure));
}
