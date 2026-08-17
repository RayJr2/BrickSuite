#pragma once

#include <QString>

namespace AppVersion
{
inline QString version()
{
    return QStringLiteral(BRICKSUITE_VERSION);
}

inline QString displayVersion()
{
    return QString("BrickSuite Version %1").arg(version());
}
}