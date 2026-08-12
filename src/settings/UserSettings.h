#pragma once

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

    static QString themeToString(Theme theme);
    static Theme themeFromString(const QString& value);

    QString rebrickableApiKey() const;
    void setRebrickableApiKey(const QString& apiKey);

private:
    UserSettings() = default;

    UserSettings(const UserSettings&) = delete;
    UserSettings& operator=(const UserSettings&) = delete;

    static constexpr int DefaultResultsPerPage = 250;
};