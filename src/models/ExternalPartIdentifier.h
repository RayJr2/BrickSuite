#pragma once
#include <QString>

struct ExternalPartIdentifier
{
    int id = 0;
    int partId = 0;
    QString provider;
    QString externalId;
    QString source;
    bool isActive = true;
};
