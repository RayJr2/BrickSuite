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

#include <QMainWindow>
#include <QStatusBar>
#include <QVector>
#include <QWidget>

class LargeViewLoadingGuard
{
public:
    LargeViewLoadingGuard(QWidget* owner, bool& refreshInProgress,
                          const QString& message,
                          const QList<QWidget*>& controlsToRestore,
                          const QList<QWidget*>& controlsRecomputedByRefresh = {})
        : m_refreshInProgress(refreshInProgress), m_message(message)
    {
        if (m_refreshInProgress)
            return;
        m_refreshInProgress = true;
        m_active = true;
        for (QWidget* control : controlsToRestore) {
            if (!control)
                continue;
            m_controls.append({control, control->isEnabled()});
            control->setEnabled(false);
        }
        for (QWidget* control : controlsRecomputedByRefresh) {
            if (control)
                control->setEnabled(false);
        }
        if (auto* window = qobject_cast<QMainWindow*>(owner ? owner->window() : nullptr)) {
            m_statusBar = window->statusBar();
            m_previousMessage = m_statusBar->currentMessage();
            m_statusBar->showMessage(m_message);
            // This synchronous, status-bar-only repaint makes the message visible
            // without processing user input or permitting a re-entrant refresh.
            m_statusBar->repaint();
        }
    }

    ~LargeViewLoadingGuard()
    {
        if (!m_active)
            return;
        for (const ControlState& state : m_controls) {
            if (state.control)
                state.control->setEnabled(state.wasEnabled);
        }
        if (m_statusBar && m_statusBar->currentMessage() == m_message) {
            if (m_previousMessage.isEmpty())
                m_statusBar->clearMessage();
            else
                m_statusBar->showMessage(m_previousMessage);
        }
        m_refreshInProgress = false;
    }

    bool active() const { return m_active; }

private:
    struct ControlState { QWidget* control = nullptr; bool wasEnabled = false; };
    bool& m_refreshInProgress;
    QString m_message;
    QString m_previousMessage;
    QStatusBar* m_statusBar = nullptr;
    QVector<ControlState> m_controls;
    bool m_active = false;
};
