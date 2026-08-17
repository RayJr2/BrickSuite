#pragma once

#include <QByteArray>
#include <QString>

class UserSettings
{
public:
    enum class Theme { System, Light, Dark };

    static UserSettings& instance();

    Theme theme() const;
    void setTheme(Theme theme);

    int resultsPerPage() const;
    void setResultsPerPage(int resultsPerPage);

    int defaultWorkspaceId() const;
    void setDefaultWorkspaceId(int workspaceId);

    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

    QByteArray logViewerGeometry() const;
    void setLogViewerGeometry(const QByteArray& geometry);

    bool showArchivedBuilds() const;
    void setShowArchivedBuilds(bool showArchived);

    static QString themeToString(Theme theme);
    static Theme themeFromString(const QString& value);

    QString rebrickableApiKey() const;
    void setRebrickableApiKey(const QString& apiKey);

    static constexpr int DefaultRebrickableRequestIntervalMs = 1250;
    static constexpr int MinimumRebrickableRequestIntervalMs = 1000;
    static constexpr int MaximumRebrickableRequestIntervalMs = 60000;

    int rebrickableMinimumRequestIntervalMs() const;

    void setRebrickableMinimumRequestIntervalMs(int intervalMs);

private:
    UserSettings() = default;

    UserSettings(const UserSettings&) = delete;
    UserSettings& operator=(const UserSettings&) = delete;

    static constexpr int DefaultResultsPerPage = 250;
};