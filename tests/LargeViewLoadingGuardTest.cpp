#include "../src/ui/helpers/LargeViewLoadingGuard.h"

#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <cstdio>

namespace {
bool require(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "%s\n", message);
    return value;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QMainWindow window;
    QWidget owner;
    window.setCentralWidget(&owner);
    QPushButton enabledControl(&owner);
    QPushButton disabledControl(&owner);
    disabledControl.setEnabled(false);
    bool refreshInProgress = false;

    window.statusBar()->showMessage("Ready");
    {
        LargeViewLoadingGuard loading(&owner, refreshInProgress, "Loading page 2...",
                                      {&enabledControl, &disabledControl});
        if (!require(loading.active(), "First loading guard should be active")
            || !require(refreshInProgress, "Refresh flag should be set")
            || !require(window.statusBar()->currentMessage() == "Loading page 2...",
                        "Loading text should be assigned immediately")
            || !require(!enabledControl.isEnabled() && !disabledControl.isEnabled(),
                        "Controls should be disabled while loading"))
            return 1;

        LargeViewLoadingGuard duplicate(&owner, refreshInProgress, "Duplicate", {});
        if (!require(!duplicate.active(), "A duplicate refresh should be rejected"))
            return 1;

        window.statusBar()->showMessage("Refresh completed");
    }

    if (!require(!refreshInProgress, "Refresh flag should be cleared")
        || !require(enabledControl.isEnabled() && !disabledControl.isEnabled(),
                    "Original control states should be restored")
        || !require(window.statusBar()->currentMessage() == "Refresh completed",
                    "A newer status message should be preserved"))
        return 1;

    window.statusBar()->showMessage("Ready");
    {
        LargeViewLoadingGuard loading(&owner, refreshInProgress, "Loading Parts Catalog...",
                                      {&enabledControl});
    }
    if (!require(window.statusBar()->currentMessage() == "Ready",
                 "The prior status should return after temporary loading"))
        return 1;

    QPushButton previous(&owner);
    QPushButton next(&owner);
    const auto verifyPaging = [&](int page, int totalPages,
                                  bool expectedPrevious, bool expectedNext) {
        previous.setEnabled(!expectedPrevious);
        next.setEnabled(!expectedNext);
        {
            LargeViewLoadingGuard loading(&owner, refreshInProgress, "Loading page...", {},
                                          {&previous, &next});
            previous.setEnabled(page > 0);
            next.setEnabled(page + 1 < totalPages);
        }
        return previous.isEnabled() == expectedPrevious
            && next.isEnabled() == expectedNext;
    };

    return require(verifyPaging(0, 3, false, true),
                   "First-page paging state was overwritten")
        && require(verifyPaging(1, 3, true, true),
                   "Middle-page paging state was overwritten")
        && require(verifyPaging(2, 3, true, false),
                   "Last-page paging state was overwritten")
        && require(verifyPaging(0, 1, false, false),
                   "Single-page paging state was overwritten") ? 0 : 1;
}
