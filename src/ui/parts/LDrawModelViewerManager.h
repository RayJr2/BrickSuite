#pragma once
#include "LDrawModelViewerWindow.h"
class LDrawModelViewerManager
{
public:
    static void showPart(const LDrawModelViewerRequest& request);
    static void shutdown();
};
