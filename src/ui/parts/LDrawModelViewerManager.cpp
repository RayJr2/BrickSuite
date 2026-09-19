#include "LDrawModelViewerManager.h"
#include "PrintPreparationCoordinator.h"
#include <QCoreApplication>
#include <QTimer>
#include <QWindow>
#include <QPointer>
namespace { QPointer<LDrawModelViewerWindow> viewer;PrintPreparationCoordinator* coordinator=nullptr; }
void LDrawModelViewerManager::showPart(const LDrawModelViewerRequest&request)
{
    if(!coordinator)coordinator=new PrintPreparationCoordinator(QCoreApplication::instance());
    if(!viewer){viewer=new LDrawModelViewerWindow(coordinator);viewer->setAttribute(Qt::WA_DeleteOnClose);QObject::connect(viewer,&QObject::destroyed,[]{viewer=nullptr;});}
    viewer->showPart(request);
    if(viewer->isMinimized())viewer->showNormal();else viewer->show();
    viewer->raise();viewer->activateWindow();if(viewer->windowHandle())viewer->windowHandle()->requestActivate();
    QPointer<LDrawModelViewerWindow> requested=viewer;
    QTimer::singleShot(0,viewer,[requested]{if(!requested)return;if(requested->isMinimized())requested->showNormal();requested->raise();requested->activateWindow();if(requested->windowHandle())requested->windowHandle()->requestActivate();});
}
