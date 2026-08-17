#pragma once

#include <QString>

namespace AppConstants {

inline QString name()
{
    return QString::fromLatin1(APP_NAME);
}

inline QString company()
{
    return QString::fromLatin1(APP_COMPANY);
}

inline QString OrganizationName()
{
    return QString::fromLatin1(APP_ORGANIZATION_NAME);
}

inline QString domain()
{
    return QString::fromLatin1(APP_DOMAIN);
}

inline QString copyrightYear()
{
    return QString::fromLatin1(APP_COPYRIGHT_YEAR);
}

inline QString release()
{
    return QString::fromLatin1(APP_RELEASE);
}

inline QString releaseDate()
{
    return QString::fromLatin1(APP_RELEASE_DATE);
}

} // namespace AppConstants