#pragma once
#include "../../models/ExternalPartIdentifier.h"
#include <QList>
#include <QStringList>
struct LDrawIdentitySelection
{
    static QStringList exactCandidates(const QList<ExternalPartIdentifier>& rows)
    {
        QStringList result;
        for(const auto& row:rows)
            if(row.isActive && row.provider.compare(QStringLiteral("LDraw"),Qt::CaseInsensitive)==0
               && !row.externalId.trimmed().isEmpty()) result<<row.externalId.trimmed();
        result.removeDuplicates(); result.sort(Qt::CaseInsensitive); return result;
    }
};
