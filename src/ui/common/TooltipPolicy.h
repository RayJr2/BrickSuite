#pragma once

#include "../../settings/UserSettings.h"
#include <QApplication>
#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QToolButton>
#include <QToolTip>

// Only registered explanatory help follows the preference. Unregistered data,
// overflow text and diagnostics remain available. Keep the text for re-enabling.
class TooltipPolicy : public QObject
{
public:
    explicit TooltipPolicy(QApplication& application) : QObject(&application) {
        application.installEventFilter(this);
    }
    static void explain(QWidget* widget, const QString& text) {
        widget->setProperty("brickSuiteExplanatoryTooltip", text);
        widget->setToolTip(text);
        widget->setAccessibleDescription(text);
    }
    static void explain(QAction* action, const QString& text) {
        action->setProperty("brickSuiteExplanatoryTooltip", text);
        action->setToolTip(text);
        action->setStatusTip(text);
    }
    static void preferenceChanged() {
        if (!UserSettings::instance().explanatoryTooltipsEnabled())
            QToolTip::hideText();
    }
protected:
    bool eventFilter(QObject* target, QEvent* event) override {
        if (event->type() != QEvent::ToolTip || UserSettings::instance().explanatoryTooltipsEnabled())
            return false;
        const auto registered = [](const QObject* object, const QString& text) {
            return object && !text.isEmpty()
                && object->property("brickSuiteExplanatoryTooltip").toString() == text;
        };
        auto* widget = qobject_cast<QWidget*>(target);
        bool explanatory = widget && registered(widget, widget->toolTip());
        if (auto* button = qobject_cast<QToolButton*>(target)) {
            const auto* action = button->defaultAction();
            explanatory |= action && registered(action, action->toolTip());
        }
        if (auto* menu = qobject_cast<QMenu*>(target)) {
            const auto* action = menu->activeAction();
            explanatory |= action && registered(action, action->toolTip());
        }
        if (explanatory) {
            QToolTip::hideText();
            return true;
        }
        return false;
    }
};
