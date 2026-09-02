#pragma once

#include <QString>

class ImageUnavailableCache
{
public:
    enum class MarkResult { Created, AlreadyKnown, Failed };

    static bool isKnownUnavailable(const QString& cacheDirectory,
                                   const QString& providerIdentity,
                                   const QString& imageUrl);
    static MarkResult markUnavailable(const QString& cacheDirectory,
                                      const QString& providerIdentity,
                                      const QString& imageUrl);

private:
    static QString markerPath(const QString& cacheDirectory,
                              const QString& providerIdentity,
                              const QString& imageUrl);
};
