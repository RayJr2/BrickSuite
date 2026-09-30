# macOS initial window activation soak fix

Baseline: `a69d79fefa5430ed22059e7345a99b99e0b87a62`, clean working tree.
Validated on macOS 26.7 with Qt 6.10.3, Release ARM64/minimum macOS 13.0.
No commit or push.

## Startup diagnosis

`main.cpp` shows the splash and processes events before synchronous application
initialization. `Application::initialize()` constructs and shows MainWindow,
starts the existing background services and returns. `main.cpp` finishes the
splash and enters the event loop. There was no explicit final main-window raise
or activation request. This leaves foreground activation dependent on the early
Qt/macOS launch handling, which can run while only the splash exists.

Inspection of the installed Qt 6.10.3 source confirms:

- Cocoa's `applicationDidFinishLaunching` attempts application activation.
- `QWidget::activateWindow()` delegates to `QWindow::requestActivate()`; Cocoa
  makes the view first responder and the native window key.
- Cocoa's `raise()` orders the visible window forward and, by default, calls
  `activateIgnoringOtherApps:YES` to activate the application itself.

Ray reported launches behind Qt Creator. The unchanged launch attempted during
this task came forward, as Ray confirmed, so the original failure was **not
reproduced deterministically**. The confirmed code gap is missing final startup
activation; a specific OS scheduling race or Qt Creator defect is not proven.
No focus suppression, always-on-top requirement or absent native-window creation
was found in BrickSuite's main-window startup.

## Correction and scope

After `splash.finish()`, call `requestInitialWindowActivation()` once. On macOS,
it queues one zero-delay callback with MainWindow as the QObject lifetime
context. The callback checks visibility and native-window existence, calls
`raise()` and then `activateWindow()`. It adds no sleep, polling, recurring timer,
show-event handler or application-state handler. The callback is cancelled if
the window is destroyed. It does not set window flags.

The helper is a no-op on Windows and Linux. No application activation is added
to provider completion, refresh, image loading or secondary-window paths.
Worker entry points still exit before QApplication and normal startup.

## Validation

- Release application and package-probe builds passed.
- `InitialWindowActivation` CTest passed (0.39 seconds). It uses Qt's offscreen
  plugin and checks queued dispatch, one macOS ZOrderChange request, no further
  requests after refresh/hide/show, hidden-window rejection, destruction safety,
  and absence of `WindowStaysOnTopHint`. It never asserts desktop stacking.
  Other platforms check that the helper makes no initial request.
- Ray confirmed the corrected Qt Creator Run became foreground and that switching
  to another app did not cause BrickSuite to take focus back. F1 opened Help.
- Qt Creator's existing Deploy path succeeded. The extracted ZIP passed deep/strict
  signature verification. Package probes passed Schema 35, Protocol 1.5, TLS,
  MCUT/Local Override workers, resources and runtime dependency closure.
- The actual extracted `.app` was opened through Finder. Ray confirmed foreground
  activation. Its startup log had zero warnings and zero critical entries.
- `git diff --check` passed. The full geometry suite was not rerun for this
  startup-only change. This task adds one focused test; it does not remove tests.
- Windows/Linux execution was not performed; their production helper is compiled
  as a no-op. Native macOS 13 execution was not performed; package auditing
  continues to enforce that deployment target.

## Single instance

The existing `QLockFile` path, stale-lock policy and duplicate-launch dialog are
unchanged. There is no existing cross-process foreground IPC in BrickSuite.
Finder reopening uses the normal macOS/Qt reopen mechanism; it retained the same
running process during this test. A direct executable invocation follows the
existing lock-and-information-dialog behavior, not a new process-to-process
activation protocol. The lock is not bypassed and no second application instance
is allowed to initialize.

Ray confirmed that Finder restored the minimized window and that direct duplicate
invocation displayed "already running". The duplicate exited normally after
dismissal. No remaining focus problem was observed in the corrected launch,
switching and reopen checks. Because the original symptom was intermittent,
continued ordinary soak use remains valuable.

## Files and invariants

- `src/main.cpp`: one startup call and helper include.
- `src/ui/common/InitialWindowActivation.h`: macOS-only deferred activation.
- `tests/InitialWindowActivationTest.cpp`: offscreen dispatch/lifetime regression.
- `CMakeLists.txt`: register the focused excluded-from-ALL test.
- `docs/macos-initial-window-activation.md`: this evidence and limitations.

Schema 35, Protocol 1.5, geometry/fit behavior, Keychain, dependency layout,
signing/notarization and other-platform startup behavior are unchanged.
No persistent always-on-top behavior or repeated activation mechanism was added.
