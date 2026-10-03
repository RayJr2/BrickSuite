#pragma once

#include <QAction>
#include <QMenu>

namespace ToolsMenuLayout
{
// Reorder the existing actions without replacing their connections or state.
inline void apply(QMenu* menu, QAction* importData, QAction* partReference,
                  QAction* viewer, QAction* calibration, QAction* database,
                  QAction* referenceData)
{
    const QList<QAction*> primary{importData, partReference, viewer, calibration, database, referenceData};
    QList<QAction*> diagnostics;
    for (auto* action : menu->actions()) {
        menu->removeAction(action);
        if (action->isSeparator()) delete action;
        else if (!primary.contains(action)) diagnostics.append(action);
    }
    menu->addAction(importData);
    menu->addSeparator();
    menu->addAction(partReference);
    menu->addSeparator();
    menu->addAction(viewer);
    menu->addAction(calibration);
    menu->addSeparator();
    menu->addAction(database);
    menu->addAction(referenceData);
    if (!diagnostics.isEmpty()) {
        menu->addSeparator();
        menu->addActions(diagnostics);
    }
}
}
