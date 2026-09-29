#include "../src/services/geometry/print/BoundedGeometryWorker.h"

// Reuse the established 100,000-transition Linux regression in a bounded child
// of the existing portable MCUT test, without changing platform test counts.
#define main mcutQueueWakeupRegression
#include "McutQueueWakeupTest.cpp"
#undef main

int mcutQueueWakeupChild()
{
    if(!PrintGeometry::BoundedGeometryWorker::constrainChild())return 2;
    return mcutQueueWakeupRegression();
}
