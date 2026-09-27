# M37 3D Printing UX and Help closeout

## Display semantics

`FitCalibrationExperiment::modeledOrientationIdentity` describes the intended
fixture. `FitCalibrationSession::process.actualPrintedOrientation` is the recorded
physical orientation. Both feature and session display names now prefer the latter
when present and otherwise display the modeled parallel/perpendicular orientation.
Other/unsupported actual evidence remains explicit; records missing both display
`Legacy orientation unspecified`. No source, session JSON, observation, identity,
verification policy or historical evidence is rewritten for presentation.

The Actual orientation combo says `Not yet confirmed` for `Unknown`. A known
intended orientation in a label is not confirmation. Existing stored actual values
remain authoritative; this closeout does not reinterpret historical values.

## Tooltips and contextual Help

Settings → Appearance → Show explanatory tooltips defaults to enabled and stores
`Appearance/ExplanatoryTooltips` using the existing `UserSettings`/`QSettings` path.
The application event filter suppresses Qt tooltip popups when disabled, without
clearing their text, disabling controls, changing status text or disabling F1.
It covers already-open and dynamically created widgets as well as future windows.

Calibration tooltips explain workspaces, generation modes, import/recovery, managed
save versus portable export, independent feature sessions, actual orientation,
observations, Preferred versus Verified, continuation and Fit Profile contribution.
Viewer and audit tooltips cover preparation, Source/Prepared, scale, repair imports,
nominal acceptance, Experimental Auto Fit, sampling, output and safe stopping.
Existing dynamic prerequisite/validation diagnostics remain available.

F1 uses the existing `HelpManager` topic properties, resolving the focused widget
and its ancestors. Calibration routes to its dedicated topic; the viewer defaults
to LDraw, preparation/export controls to Prepare for Printing, repair controls and
review warnings to Local Printable Override, and the audit to Troubleshooting.
Unassigned contexts retain the MainWindow fallback. Existing topic IDs and anchors
are preserved; the new topic IDs are appended.

## Authoritative Help structure

The top-level **3D Printing** tree contains Overview, LDraw 3D Models, Prepare for
Printing, LEGO Fit Calibration, Calibration Packages & Fit Profiles, Local
Printable Override / External Repair, and Print Capability / Troubleshooting.
Content was moved from the former monolithic LDraw page, with links between focused
topics. `resources/help` remains the sole published/built-in content source.
Getting Started provides a short discovery link rather than duplicating the manual.

The original Help search only filtered topic results; it did not inject HTML marks
or text-search selections. The LDraw page's pale `code` background inherited the
Dark theme's light foreground. The split pages remove that theme-specific
background. Real search matches now use `QTextBrowser` extra selections with a
palette-derived background and a contrasting black/white foreground (at least
4.5:1), refreshed on search, navigation and theme change. Clearing search removes
them. Each resource page is indexed once.

## Ray's runtime acceptance

1. Import the existing custom `package-session.json` in LEGO Fit Calibration.
2. Confirm the Standard Stud identity says Perpendicular rather than Orientation
   Unknown when its modeled orientation is known.
3. Confirm Actual orientation still says Not yet confirmed for unconfirmed data;
   explicitly recorded physical orientation must remain unchanged.
4. Focus a calibration control and press F1.
5. Confirm Help opens directly to LEGO Fit Calibration.
6. Inspect the seven focused pages under the new 3D Printing tree.
7. Search Help for `printing`.
8. Confirm multiple relevant topics appear and selecting one retains highlights.
9. In Dark theme, check highlighted text and inline code are readable.
10. Switch to Light theme, keeping the search active, and check readability again.
11. Apply Settings → Appearance → Show explanatory tooltips off, then on.
12. Hover workspace, actual orientation, Preferred/Verified, repair and audit
    controls. With tooltips disabled, controls, statuses and F1 still work.
13. Restart BrickSuite and confirm the saved tooltip preference is retained.

Also check F1 on viewer preparation/export and repair-review controls. Broad Help
screenshots, release visual polish and the README audit remain M39 work. No
calibration geometry, package composition, lineage, profile merging, override
acceptance, ManufacturingMesh corrections or audit processing changes belong to
this closeout. Earlier uncommitted finalization work is preserved separately.

## Validation

Qt 6.10.3 MinGW Release configure/build and explicit builds of all 114 configured
test executables passed. The focused Help/calibration/settings/viewer selection
passed 8/8. Full configured CTest passed 120/120 in 161.40 seconds with four workers,
including all six installed-LDraw integrations. The new Help test covers a real F1
key event, topic indexing/navigation, live Dark/Light highlight changes and contrast,
tooltip suppression/re-enabling, and preference restoration in a separate process.
Other focused tests cover modeled/actual/legacy orientation, unchanged session JSON,
package members, and actual viewer/audit context assignments.

All seven printing pages have compiled resource entries and resolving relative
links. Working-tree and staged whitespace checks passed. Schema 35 and Protocol
1.5 are unchanged. No Debug build or broad screenshot refresh was performed.
Ray's interactive acceptance checklist above remains separate from these automated
results. No commit or push was made.
