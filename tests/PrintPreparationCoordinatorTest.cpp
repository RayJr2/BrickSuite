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

#include "../src/ui/parts/PrintPreparationCoordinator.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

namespace { bool check(bool value,const char*message){if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);bool ok=true;PrintPreparationCoordinator coordinator;PrintGeometry::PrintPreparationRequest request;request.partReference="invalid";request.ldrawIdentity="invalid";request.libraryAuthority="test";
    QEventLoop loop;quint64 deliveredGeneration=0;PrintGeometry::PrintPreparationResult delivered;
    QObject::connect(&coordinator,&PrintPreparationCoordinator::completed,&loop,[&](quint64 generation,const auto&result){deliveredGeneration=generation;delivered=result;loop.quit();});
    ok&=check(coordinator.start(42,request),"first asynchronous request accepted");ok&=check(coordinator.busy(),"coordinator reports active job");ok&=check(!coordinator.start(43,request),"duplicate active job rejected");
    QTimer::singleShot(5000,&loop,&QEventLoop::quit);loop.exec();
    ok&=check(deliveredGeneration==42,"request generation preserved");ok&=check(delivered.state==PrintGeometry::PrintPreparationState::Failed,"worker result delivered safely");ok&=check(!coordinator.busy(),"active state cleared after completion");
    ok&=check(coordinator.start(44,request),"coordinator reusable after completion");coordinator.cancel();QEventLoop cancelled;QObject::connect(&coordinator,&PrintPreparationCoordinator::completed,&cancelled,[&](quint64,const auto&){cancelled.quit();});QTimer::singleShot(5000,&cancelled,&QEventLoop::quit);cancelled.exec();ok&=check(!coordinator.busy(),"cancelled request cleaned up");
    return ok?0:1;
}
