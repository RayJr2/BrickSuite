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

#include "LDrawModelViewerManager.h"
#include "PrintPreparationCoordinator.h"
#include <QCoreApplication>
#include <QTimer>
#include <QWindow>
#include <QPointer>
namespace { QPointer<LDrawModelViewerWindow> viewer;PrintPreparationCoordinator* coordinator=nullptr;bool shutdownConnected=false; }
void LDrawModelViewerManager::showPart(const LDrawModelViewerRequest&request)
{
    if(!coordinator)coordinator=new PrintPreparationCoordinator(QCoreApplication::instance());
    if(!shutdownConnected&&QCoreApplication::instance()){QObject::connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,[]{LDrawModelViewerManager::shutdown();});shutdownConnected=true;}
    if(!viewer){viewer=new LDrawModelViewerWindow(coordinator);viewer->setAttribute(Qt::WA_DeleteOnClose);QObject::connect(viewer,&QObject::destroyed,[]{viewer=nullptr;});}
    viewer->showPart(request);
    if(viewer->isMinimized())viewer->showNormal();else viewer->show();
    viewer->raise();viewer->activateWindow();if(viewer->windowHandle())viewer->windowHandle()->requestActivate();
    QPointer<LDrawModelViewerWindow> requested=viewer;
    QTimer::singleShot(0,viewer,[requested]{if(!requested)return;if(requested->isMinimized())requested->showNormal();requested->raise();requested->activateWindow();if(requested->windowHandle())requested->windowHandle()->requestActivate();});
}
void LDrawModelViewerManager::shutdown()
{
    if(coordinator)coordinator->cancel();
    if(viewer)viewer->close();
}
