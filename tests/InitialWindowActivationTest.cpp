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

#include "../src/ui/common/InitialWindowActivation.h"

#include <QApplication>
#include <QEvent>
#include <cstdio>

namespace {
class Window : public QWidget
{
public:
    int raises = 0;
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ZOrderChange) ++raises;
        return QWidget::event(event);
    }
};

bool check(bool condition, const char* message)
{
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}
}

int main(int argc, char** argv)
{
    // Check dispatch/lifetime rules, never actual desktop stacking or focus.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Window window;
    window.show();
    app.processEvents();
    const int before = window.raises;
    requestInitialWindowActivation(&window);
    bool ok = check(window.raises == before, "activation is deferred");
    app.processEvents();
#ifdef Q_OS_MACOS
    ok &= check(window.raises == before + 1, "one initial macOS raise request");
#else
    ok &= check(window.raises == before, "other platform startup unchanged");
#endif
    const int after = window.raises;
    window.update();
    window.hide();
    window.show();
    app.processEvents();
    app.processEvents();
    ok &= check(window.raises == after, "refresh and subsequent show do not re-activate");
    ok &= check(!window.windowFlags().testFlag(Qt::WindowStaysOnTopHint), "normal window ordering retained");

    Window hidden;
    hidden.show();
    app.processEvents();
    requestInitialWindowActivation(&hidden);
    hidden.hide();
    const int hiddenBefore = hidden.raises;
    app.processEvents();
    ok &= check(hidden.raises == hiddenBefore, "hidden window is not raised by pending startup request");

    auto* destroyed = new Window;
    destroyed->show();
    requestInitialWindowActivation(destroyed);
    delete destroyed;
    app.processEvents(); // QObject context cancels the callback on destruction.
    return ok ? 0 : 1;
}
