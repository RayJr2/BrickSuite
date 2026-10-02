# Set Details provider layout soak fix

Baseline: `d99d8d8` (clean working tree). Native ARM64, macOS 26.7,
Qt 6.10.3. No commit or push.

## Cause and measured reproduction

Provider Enrichment used QFormLayout's style defaults. Qt 6.10.3's macOS style
selects `FieldsStayAtSizeHint`, horizontal form centering, right-aligned labels
and `DontWrapRows`. The Status QLabel allowed word wrapping, but the field was
capped at its size hint instead of using the available pane width. The issue
was field growth, **not** QFormLayout wrapping labels above their fields, font
size or a non-resizable scroll area.

The real SetDetailsDialog was exercised with an isolated 42118-1 / Monster Jam
Grave Digger fixture and a Brickset result delivered through its existing
provider signal. No provider credentials or live HTTP calls were used. The
fixture reproduced the reported narrow centered form:

| Dialog size | Native pane width | Old Status width | New Status width |
| --- | ---: | ---: | ---: |
| 1000 × 750 | 943 | 273 | 794 |
| 1200 × 800 | 1143 | 273 | 994 |
| 1600 × approximately 945 (screen-limited) | 1543 | 273 | 1394 |

The normal loaded-status text needs 336 pixels with the tested native font.
Thus it wrapped at every old width despite hundreds of unused pixels. The
scroll area's existing `widgetResizable(true)` already expanded its child.
There were no field maximum widths or splitter width constraints to remove.

## Shared layout correction

- Form policy: explicit `ExpandingFieldsGrow`, `DontWrapRows`, left/top form
  alignment and left/vertically centered labels.
- Provider group and field widgets: horizontally expanding size policies.
- Values: left-aligned, using the available column width. Word wrapping remains
  available for genuinely long text rather than eliding, clipping or imposing
  a large dialog minimum width. The normal Status now fits one line.
- Instructions: keep the count and button together, with surplus space after
  the button rather than between them.

No macOS conditional, fixed/minimum-width pixel tuning, typography change or
provider-data change was added. Existing margins and style spacing remain.
The change makes the already-good Windows/Linux expanding-form behavior explicit;
native Windows/Linux visual validation was not performed during this Mac task.

## Native and regression results

- Release and Debug application builds passed using the macOS 13-target Qt kit.
- `SetDetailsInventoryWorkflow` passed under CTest/offscreen (0.63 seconds).
  The same final test passed with native Cocoa widgets.
- Status fits one line at all three normal widths, including after live theme
  changes. A long Status uses its complete 48-pixel wrapped height; long Subtheme
  and Availability values each use their full 32-pixel height. Horizontal scroll
  range remains zero. These are measured outcomes, not hard-coded layout sizes.
- Native visual inspection covered 1000 × 750, 1200 × 800, larger and macOS
  maximized windows. Source, Status, Brickset ID, Theme, Subtheme, Minifigs,
  Availability, Rating, Instructions, Additional Images and Provider Link are
  aligned and readable. `Instructions: 3` and View Instructions stay together.
- Dark → Light → Dark was exercised while the dialog remained open. The
  ThemeRichTextLabel implementation is untouched; tests verify changing link
  color, stable markup on return to Dark, retained URL and external-link policy.
  The external website and live instructions API were not invoked by the fixture.
- The default splitter leaves most of the available section height to Catalog
  Parts. Native dragging smaller/larger works. Vertical scrolling remains when
  the provider pane is short and disappears when all rows fit. The existing
  `SetDetails/sectionsSplitterState` key and save/restore implementation are
  untouched. Both collapsed and non-collapsed state restoration pass on reopen.
- Catalog headers, row selection, table scrolling, modeless dialog lifetime and
  Add Inventory action remain covered by the real workflow test. Actual saves
  verify exact Part/Color/storage/quantity, unchanged catalog composition,
  selection/scroll retention, Keep Open, Remember Part and Remote rejection.
- A native manual fixture save was also performed: select a Catalog Part, open
  Add Inventory, save quantity 1 to its active fixture bin, and return to the
  still-open Set Details. Read-only inspection confirmed quantity 1 in the
  isolated database; the selected row and splitter remained stable. No real
  inventory was changed.

The first long-value assertion read geometry before the scroll area's queued
LayoutRequest was processed. Draining that propagation fixed the test; no timer,
forced repaint or extra production workaround was introduced. A sandboxed test
could not create its isolated database; the authorized external run passed.

## Files and scope

- `src/ui/catalog/SetDetailsDialog.cpp`: provider layout policies only.
- `tests/SetDetailsInventoryWorkflowTest.cpp`: enrichment/layout/theme cases,
  non-collapsed persistence check and optional isolated native preview.
- `CMakeLists.txt`: link the existing ThemeManager implementation into that test.
- `docs/macos-set-details-provider-layout.md`: this report.

`git diff --check` passes. Schema 35 and Protocol 1.5 are unchanged, as are M37
geometry/fit, Set composition, provider/API logic, Add Inventory transactions,
Remote behavior, packaging and Support BrickSuite. No full-suite rerun was needed
for this scoped UI change. Windows/Linux should rerun the focused workflow test
and visually inspect the shared layout with their native styles.

No remaining layout defect was observed in the native fixture. Ray's live
provider-backed database was not modified or used for fixture saves; acceptance
against that data remains a normal soak check on the rebuilt application.
