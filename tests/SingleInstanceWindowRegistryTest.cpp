#include "../src/ui/common/SingleInstanceWindowRegistry.h"

#include <QCoreApplication>
#include <QDebug>
#include <QObject>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) qCritical() << message;
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    SingleInstanceWindowRegistry<int, QObject> registry;
    QObject imageNotifier;
    int factoryCalls = 0;
    int imageUpdates = 0;

    auto open = [&]() {
        if (QObject* existing = registry.find(42))
            return existing;
        auto* window = new QObject;
        ++factoryCalls;
        registry.track(42, window);
        QObject::connect(&imageNotifier, &QObject::objectNameChanged, window,
                         [&](const QString&) { ++imageUpdates; });
        return window;
    };

    QObject* first = open();
    for (int i = 0; i < 4; ++i) {
        imageNotifier.setObjectName(QString::number(i));
        if (!require(open() == first, "Image arrival created another window.")) return 1;
    }
    if (!require(factoryCalls == 1 && imageUpdates == 4,
                 "The first window did not exclusively receive image updates.")) return 1;

    delete first;
    imageNotifier.setObjectName(QStringLiteral("after-close"));
    if (!require(imageUpdates == 4, "An image callback ran after window destruction.")) return 1;

    QObject* reopened = open();
    if (!require(reopened && factoryCalls == 2 && registry.find(42) == reopened,
                 "Closing then reopening did not create exactly one fresh window.")) return 1;
    delete reopened;
    return 0;
}
