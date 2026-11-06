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

#include "SupportLinks.h"
#include <QApplication>
#include <QEvent>
#include <QMessageBox>

namespace SupportLinks {
#ifdef BRICKSUITE_SUPPORT_TESTING
static QPointer<Coordinator> testCoordinator;
void setTestCoordinator(Coordinator* value) { testCoordinator = value; }
#endif
Coordinator* coordinator()
{
#ifdef BRICKSUITE_SUPPORT_TESTING
    if (testCoordinator) return testCoordinator;
#endif
    static QPointer<Coordinator> shared;
    if (!shared) shared = new Coordinator(qApp);
    return shared;
}
Coordinator::Coordinator(QObject* parent, SupportAvailabilityService* selector)
    : QObject(parent), m_selector(selector ? selector : new SupportAvailabilityService(this))
{
    connect(m_selector, &SupportAvailabilityService::selected, this, &Coordinator::selected);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { m_quitting = true; cancel(); });
}
Coordinator::~Coordinator() { cancel(); }
void Coordinator::cancel()
{
    m_active = false;
    m_selector->cancel();
    if (m_owner) m_owner->removeEventFilter(this);
    disconnect(m_destroyed);
    m_owner.clear();
    m_opener = {};
}
bool Coordinator::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_owner && (event->type() == QEvent::Close || event->type() == QEvent::Hide)) cancel();
    return QObject::eventFilter(watched, event);
}
void Coordinator::start(QWidget* owner, UrlOpener opener)
{
    if (!owner || m_quitting || m_active || m_handingOff || (m_guard.isValid() && m_guard.elapsed() < 1000)) return;
    m_active = true;
    m_owner = owner->window();
    m_opener = std::move(opener);
    m_owner->installEventFilter(this);
    m_destroyed = connect(m_owner, &QObject::destroyed, this, &Coordinator::cancel);
    m_selector->start();
}
void Coordinator::selected(const QUrl& url)
{
    if (!m_active) return;
    QPointer<QWidget> owner = m_owner;
    auto opener = m_opener;
    cancel();
    m_guard.start();
    m_handingOff = true;
    QPointer<Coordinator> self(this);
    if (owner) handoff(owner, url, std::move(opener));
    if (self) m_handingOff = false;
}
void handoff(QWidget* parent, const QUrl& url, UrlOpener opener)
{
    QPointer<QWidget> owner(parent);
    if (opener(url) || !owner) return;
    QMessageBox warning(QMessageBox::Warning, QObject::tr("Support BrickSuite"),
        QObject::tr("BrickSuite could not open the Support checkout in your web browser.\n\n"
                    "You can open this address manually:\n\n%1").arg(url.toString()),
        QMessageBox::Ok, owner);
    warning.setTextFormat(Qt::PlainText);
    warning.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    warning.exec();
}
void openSupportPage(QWidget* parent, UrlOpener opener) { coordinator()->start(parent, std::move(opener)); }
QAction* addHelpAction(QMenu* helpMenu, QWidget* parent)
{
    auto* action = helpMenu->addAction(QObject::tr("Support BrickSuite…"));
    action->setObjectName(QStringLiteral("supportBrickSuiteAction"));
    action->setStatusTip(QObject::tr("Open the voluntary Support checkout in your web browser."));
    QObject::connect(action, &QAction::triggered, parent, [parent] { openSupportPage(parent); });
    return action;
}
} // namespace SupportLinks
