/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#pragma once

#include <QPointer>
#include <QSharedPointer>
#include <QTimer>
#include <QWeakPointer>

#include <functional>
#include <utility>

class MyInventoryAddSessionRefresh
{
public:
    using Callback = std::function<void()>;

    MyInventoryAddSessionRefresh(QObject* context,
                                 Callback refreshPresentation,
                                 Callback publishSessionChange)
        : m_context(context)
        , m_state(QSharedPointer<State>::create(
              State{false, false, std::move(refreshPresentation),
                    std::move(publishSessionChange)}))
    {
    }

    void inventoryAdded()
    {
        if (!m_context || !m_state)
            return;

        m_state->sessionChanged = true;
        if (m_state->refreshPending)
            return;

        m_state->refreshPending = true;
        const QWeakPointer<State> weakState(m_state);
        QTimer::singleShot(0, m_context, [weakState]() {
            const QSharedPointer<State> state = weakState.toStrongRef();
            if (!state || !state->refreshPending)
                return;

            state->refreshPending = false;
            state->refreshPresentation();
        });
    }

    void sessionFinished()
    {
        if (!m_state || !m_state->sessionChanged)
            return;

        if (m_state->refreshPending) {
            m_state->refreshPending = false;
            m_state->refreshPresentation();
        }

        m_state->sessionChanged = false;
        m_state->publishSessionChange();
    }

private:
    struct State
    {
        bool refreshPending = false;
        bool sessionChanged = false;
        Callback refreshPresentation;
        Callback publishSessionChange;
    };

    QPointer<QObject> m_context;
    QSharedPointer<State> m_state;
};
