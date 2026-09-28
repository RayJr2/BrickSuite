// Test-only entry point: exercise production startup with isolated INI settings.
// The normal release entry point and native CFPreferences backend are unchanged.
#include <QDir>
#include <QSettings>
#define main brickSuiteProductionMain
#include "src/main.cpp"
#undef main
int main(int argc, char** argv)
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                      QDir::homePath()+QStringLiteral("/package-test-settings"));
    return brickSuiteProductionMain(argc, argv);
}
