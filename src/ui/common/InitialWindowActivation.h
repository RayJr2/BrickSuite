#pragma once

#include <QTimer>
#include <QWidget>

inline void requestInitialWindowActivation(QWidget* window)
{
#ifdef Q_OS_MACOS
    // The splash can consume launch activation before the main window exists.
    // Cocoa's raise() activates the app; activateWindow() gives keyboard focus.
    // Queue once after the splash finishes, never on show or refresh events.
    QTimer::singleShot(0, window, [window] {
        if (window->isVisible() && window->windowHandle()) {
            window->raise();
            window->activateWindow();
        }
    });
#else
    Q_UNUSED(window);
#endif
}
