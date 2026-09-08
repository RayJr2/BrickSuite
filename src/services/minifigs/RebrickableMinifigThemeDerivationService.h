#pragma once

#include <QSqlDatabase>
#include <QString>

class RebrickableMinifigThemeDerivationService
{
public:
    struct Result { bool success=false; qint64 associations=0,replaced=0; QString message; };
    Result rebuild(QSqlDatabase database=QSqlDatabase()) const;
};
