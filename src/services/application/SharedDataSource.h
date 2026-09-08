#pragma once

// Stable device-local composition choice. Keep serialized spellings in
// UserSettings; this enum is shared by startup composition and presentation.
enum class SharedDataSource
{
    ThisComputer,
    BrickSuiteHost
};
