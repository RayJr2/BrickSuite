/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
