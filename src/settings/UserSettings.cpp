#include "UserSettings.h"

#include <QSettings>

namespace {
constexpr auto kGroupGeneral = "General";

constexpr auto kGroupAppearance = "Appearance";

constexpr auto kGroupRebrickable = "Rebrickable";

constexpr auto kThemeKey = "Theme";

constexpr auto kResultsPerPageKey = "ResultsPerPage";

constexpr auto kDefaultWorkspaceIdKey = "DefaultWorkspaceId";

constexpr auto kRebrickableApiKey = "ApiKey";

constexpr auto kRebrickableMinimumRequestIntervalMsKey = "MinimumRequestIntervalMs";

constexpr auto kGroupMainWindow = "MainWindow";

constexpr auto kMainWindowGeometryKey = "Geometry";
constexpr auto kMainWindowStateKey = "State";

constexpr auto kGroupLogViewer = "LogViewer";
constexpr auto kLogViewerGeometryKey = "Geometry";

} // namespace

UserSettings& UserSettings::instance()
{
    static UserSettings settings;

    return settings;
}

UserSettings::Theme UserSettings::theme() const
{
    QSettings settings;

    settings.beginGroup(kGroupAppearance);

    const QString value = settings.value(kThemeKey, "system").toString();

    settings.endGroup();

    return themeFromString(value);
}

void UserSettings::setTheme(Theme theme)
{
    QSettings settings;

    settings.beginGroup(kGroupAppearance);

    settings.setValue(kThemeKey, themeToString(theme));

    settings.endGroup();
}

int UserSettings::resultsPerPage() const
{
    QSettings settings;

    settings.beginGroup(kGroupGeneral);

    const int value = settings.value(kResultsPerPageKey, DefaultResultsPerPage).toInt();

    settings.endGroup();

    switch (value) {
    case 100:
    case 250:
    case 500:
        return value;

    default:
        return DefaultResultsPerPage;
    }
}

void UserSettings::setResultsPerPage(int resultsPerPage)
{
    switch (resultsPerPage) {
    case 100:
    case 250:
    case 500:
        break;

    default:
        resultsPerPage = DefaultResultsPerPage;
        break;
    }

    QSettings settings;

    settings.beginGroup(kGroupGeneral);

    settings.setValue(kResultsPerPageKey, resultsPerPage);

    settings.endGroup();
}

int UserSettings::defaultWorkspaceId() const
{
    QSettings settings;

    settings.beginGroup(kGroupGeneral);

    const int workspaceId = settings.value(kDefaultWorkspaceIdKey, 0).toInt();

    settings.endGroup();

    return workspaceId;
}

void UserSettings::setDefaultWorkspaceId(int workspaceId)
{
    if (workspaceId < 0)
        workspaceId = 0;

    QSettings settings;

    settings.beginGroup(kGroupGeneral);

    settings.setValue(kDefaultWorkspaceIdKey, workspaceId);

    settings.endGroup();
}

QString UserSettings::rebrickableApiKey() const
{
    QSettings settings;

    settings.beginGroup(kGroupRebrickable);

    const QString apiKey = settings.value(kRebrickableApiKey).toString();

    settings.endGroup();

    return apiKey;
}

void UserSettings::setRebrickableApiKey(const QString& apiKey)
{
    QSettings settings;

    settings.beginGroup(kGroupRebrickable);

    settings.setValue(kRebrickableApiKey, apiKey.trimmed());

    settings.endGroup();
}

int UserSettings::rebrickableMinimumRequestIntervalMs() const
{
    QSettings settings;

    settings.beginGroup(kGroupRebrickable);

    int value = settings
                    .value(kRebrickableMinimumRequestIntervalMsKey,
                           DefaultRebrickableRequestIntervalMs)
                    .toInt();

    settings.endGroup();

    if (value < MinimumRebrickableRequestIntervalMs)
        value = MinimumRebrickableRequestIntervalMs;

    if (value > MaximumRebrickableRequestIntervalMs)
        value = MaximumRebrickableRequestIntervalMs;

    return value;
}

void UserSettings::setRebrickableMinimumRequestIntervalMs(int intervalMs)
{
    if (intervalMs < MinimumRebrickableRequestIntervalMs)
        intervalMs = MinimumRebrickableRequestIntervalMs;

    if (intervalMs > MaximumRebrickableRequestIntervalMs)
        intervalMs = MaximumRebrickableRequestIntervalMs;

    QSettings settings;

    settings.beginGroup(kGroupRebrickable);

    settings.setValue(kRebrickableMinimumRequestIntervalMsKey, intervalMs);

    settings.endGroup();
}

QString UserSettings::themeToString(Theme theme)
{
    switch (theme) {
    case Theme::Light:
        return "light";

    case Theme::Dark:
        return "dark";

    case Theme::System:
    default:
        return "system";
    }
}

UserSettings::Theme UserSettings::themeFromString(const QString& value)
{
    if (value.compare("light", Qt::CaseInsensitive) == 0) {
        return Theme::Light;
    }

    if (value.compare("dark", Qt::CaseInsensitive) == 0) {
        return Theme::Dark;
    }

    return Theme::System;
}

QByteArray UserSettings::mainWindowGeometry() const
{
    QSettings settings;

    settings.beginGroup(kGroupMainWindow);

    const QByteArray geometry = settings.value(kMainWindowGeometryKey).toByteArray();

    settings.endGroup();

    return geometry;
}

void UserSettings::setMainWindowGeometry(const QByteArray& geometry)
{
    QSettings settings;

    settings.beginGroup(kGroupMainWindow);

    settings.setValue(kMainWindowGeometryKey, geometry);

    settings.endGroup();
}

QByteArray UserSettings::mainWindowState() const
{
    QSettings settings;

    settings.beginGroup(kGroupMainWindow);

    const QByteArray state = settings.value(kMainWindowStateKey).toByteArray();

    settings.endGroup();

    return state;
}

void UserSettings::setMainWindowState(const QByteArray& state)
{
    QSettings settings;

    settings.beginGroup(kGroupMainWindow);

    settings.setValue(kMainWindowStateKey, state);

    settings.endGroup();
}

QByteArray UserSettings::logViewerGeometry() const
{
    QSettings settings;

    settings.beginGroup(kGroupLogViewer);

    const QByteArray geometry = settings.value(kLogViewerGeometryKey).toByteArray();

    settings.endGroup();

    return geometry;
}

void UserSettings::setLogViewerGeometry(const QByteArray& geometry)
{
    QSettings settings;

    settings.beginGroup(kGroupLogViewer);

    settings.setValue(kLogViewerGeometryKey, geometry);

    settings.endGroup();
}
