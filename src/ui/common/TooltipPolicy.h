#pragma once

#include "../../settings/UserSettings.h"
#include <QApplication>
#include <QEvent>
#include <QToolTip>

// Keep text on the controls: toggling the preference also covers existing and
// dynamically created widgets, without clearing contextual diagnostics.
class TooltipPolicy : public QObject
{
public:
    explicit TooltipPolicy(QApplication& application) : QObject(&application) {
        application.installEventFilter(this);
    }
protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::ToolTip && !UserSettings::instance().explanatoryTooltipsEnabled()) {
            QToolTip::hideText();
            return true;
        }
        return false;
    }
};
