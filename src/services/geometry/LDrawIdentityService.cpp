#include "LDrawIdentityService.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "LDrawIdentitySelection.h"
QStringList LDrawIdentityService::candidatesForPart(int partId) const
{
    const ExternalPartIdentifierRepository repository = m_database.isValid()
        ? ExternalPartIdentifierRepository(m_database) : ExternalPartIdentifierRepository();
    const auto rows=repository.findByPartAndProvider(partId,QStringLiteral("LDraw"));
    return LDrawIdentitySelection::exactCandidates(rows);
}
