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

#include "../../core/AppConstants.h"
#include "../../services/SupportAvailabilityService.h"
#include <QAction>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QMenu>
#include <QPointer>
#include <functional>

namespace SupportLinks {
using UrlOpener = std::function<bool(const QUrl&)>;
void handoff(QWidget* parent, const QUrl& url, UrlOpener opener = &QDesktopServices::openUrl);

class Coordinator final : public QObject
{
public:
    explicit Coordinator(QObject* parent = nullptr, SupportAvailabilityService* selector = nullptr);
    ~Coordinator() override;
    void start(QWidget* owner, UrlOpener opener = &QDesktopServices::openUrl);
    void cancel();
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void selected(const QUrl& url);
    SupportAvailabilityService* m_selector;
    QPointer<QWidget> m_owner;
    QMetaObject::Connection m_destroyed;
    QElapsedTimer m_guard;
    UrlOpener m_opener;
    bool m_active = false;
    bool m_handingOff = false;
    bool m_quitting = false;
};
Coordinator* coordinator();
#ifdef BRICKSUITE_SUPPORT_TESTING
void setTestCoordinator(Coordinator* coordinator);
#endif
void openSupportPage(QWidget* parent, UrlOpener opener = &QDesktopServices::openUrl);
QAction* addHelpAction(QMenu* helpMenu, QWidget* parent);
} // namespace SupportLinks
