# M38 tooltip and context-help audit

## Scope and classifications

Audited against the clean M37 baseline `8a9d6f7`. The UI inventory preceded
implementation. This is a presentation-only pass: no inventory, Build, provider,
geometry, calibration, schema or protocol behavior changes.

Controls were classified as **explanation**, **no tooltip**, **status**, **F1 only**,
or **developer-only**. Ordinary OK/Cancel/Close, Search, Name, Quantity, paging,
and clearly labelled file/navigation buttons intentionally need no extra hover
text. Exceptions include Edit Inventory's quantity, which sets a total rather
than adding a quantity. Existing confirmations remain authoritative.

| Area and controls reviewed | Explanatory help / intentional omissions | F1 route and remaining documentation work |
| --- | --- | --- |
| Main window: workspaces, tabs, menus, status, global actions | Backup/restore, external LDraw opening and Part Reference actions have aligned status tips. Workspace names, add/save labels and navigation remain self-explanatory; connection failures remain status information. | Existing tab fallback preserved; no dedicated workspace topic added. M40 can expand workspace guidance. |
| Parts / My Inventory: filters, paging, row actions, resolver, add/edit/move/correct/remove/history, lost/found | Remember Part, Keep Open, Show All Colors, physical manufacturer, destination, total quantity and identity correction explain their scope. Existing Try BrickLink ID wording retained. Ownership currently offers Owned only and needs no extra tooltip. Row action combos gain accessible names. Part names, elided values and Host errors remain data tooltips. | Parts Catalog, Inventory or Lost/Found as appropriate. History uses Inventory. |
| Part Reference: family/catalog tabs, cards, selection, copy/send/find Sets, customizations | Send opens Add Inventory without saving; customizations do not mutate the catalog. Existing Host customization wording retained. Cards have full Part/name accessible names and data tooltips. Copy, tabs and search need no extra explanation. | Part Reference, including the add-customization dialog. |
| Builds: type, Stock/Complete Set, status, archive visibility, requirements, allocation/substitution, pulling/import/export, history | Stock allocates loose Inventory; Complete Set represents an intact physical Set. Allocation differs from physical pulling. MOC import creates requirements, not owned Inventory. Archive visibility does not reactivate records. Spare and pulling/import actions explain their scope. Existing substitution cells retain their actual data. | Builds for requirement editing, allocation and pulling; Missing Parts for export. Existing Disassemble context retained. M40 may add finer anchors for status/history. |
| What Can I Build: modes, criteria, availability, filters, collection sources, create-Build actions | Advisory discovery does not reserve pieces; opted-in Collection Sets are not consumed. Minimum percentage and all/any matching explained. Existing result status and invalid year-range diagnostics retained. Obvious paging/filter labels left alone. | Builds; M40 can add a dedicated discovery topic. |
| Procurement: resolution/retry, overrides, Remember, Color, conditions, XML and upload | Exact ITEMID overrides and persistence on XML generation explained; source requirements remain unchanged. XML generation does not place orders. Unlabelled table controls gain accessible names. Status columns keep actual resolution/export results. | Missing Parts for preview and XML result. M40 can expand mapping-review details. |
| Sets / Minifigs: catalog import, details, composition, provider/instruction links, Create Build | Existing composition prerequisite tooltips registered; ordinary import/link labels and provider-derived data remain unchanged. Row actions gain accessible names where present. | Sets Catalog or Minifigs Catalog; instruction dialog uses Sets Catalog. |
| Storage: hierarchy, type, capabilities, active/inactive filters, destinations | Parent versus operational leaf and physical location type explained. Existing eligibility diagnostics and confirmation/status messages retained. No disabled-widget event workaround. | Storage for management and location editor. |
| APIs / Settings: keys, connection tests, pacing, thresholds, defaults, theme, LDraw | Credential purpose and tests explained without echoing values. Existing coordination authority, daily fallback threshold, tooltip preference and M37 3D Models wording retained. Theme and obvious labels need no duplicate text. | Settings; Builds defaults and LDraw tab use their specific topics. |
| Host / Remote: source selection, enable, connection/trust, pairing, devices, maintenance, unknown/stale mutations | Trust, private credential purpose, reconnect, forget, pair, revoke and maintenance explain consequences. Existing host-authoritative stale/unknown-outcome messages remain visible status, not hidden hover help. No security values added to help. | Server settings use BrickSuite Server; mutation dialogs use their operational Inventory/Build/Storage topics. M40 can expand cross-links to retry/trust guidance. |
| Backup / restore / maintenance | Action status tips explain recovery/replacement; existing retention scope retained. Existing file pickers, confirmation and progress text remain the safety mechanism. | Database Backup settings route to Backup/Restore; existing Database Status Help preserved. Native system file pickers are outside application F1 routing. |
| Collection and exports | Existing advisory-parts-source explanation registered; existing Collection/export contexts preserved. Ordinary filter/export-column ordering controls need no duplicate text. | Existing My Collection and export anchors. |
| Importers: Inventory CSV, Parts/Sets ZIP/CSV, global Rebrickable files, MOC and pull lists | Existing Inventory operation/storage explanations registered. Review tables, validation errors and destructive confirmations remain visible. MOC/pull actions distinguish requirements from stock and recorded pulling. No implied Custom List API. | Inventory, Rebrickable Import, Builds or catalog topics as appropriate. |
| M37 viewer / printing / calibration | Existing Source/Prepared, scale, orientation, explicit profile, override, warned experimental fit, package/session, Preferred/Verified and continuation wording retained. All explanatory widget help registered. Source counts, validation diagnostics, provenance and guidance remain available as data/status. | Existing dedicated LDraw / Prepare / override / calibration contexts preserved. |
| Logs / diagnostics / Debug | Log and color review get broader contexts; errors remain status/data. Resolver/manufacturing proof remain developer-only. Print Audit's existing explanations are registered without wording or preparation changes. | Logging, Inventory, existing Print Troubleshooting. M40 may add focused color-review guidance. |

## Shared behavior

`TooltipPolicy::explain` registers text on a widget or action. The application
event filter consults the existing QSettings-backed preference at hover time,
so open and newly created controls follow changes without rebuilding dialogs.
Settings saving hides an active popup when disabling. Re-enabling needs no
text reconstruction. Actions also receive a matching status tip; widget
descriptions are available to accessibility clients even with hover disabled.

Only currently registered explanatory text is suppressed. A later diagnostic
that replaces generic help is allowed through; table-item full text and
diagnostics are not registered. Dynamic explanatory refreshes explicitly
re-register their current text (including calibration capability and provider
coordination updates). No table scanning, timers, polling or keyboard filtering
is introduced. F1 is independent of the preference. Qt and the existing theme
palette render the help; no tooltip colors are hard-coded.

Context registration is an inline property-only operation so small UI test
targets do not need to link a Help browser just to declare their context.
Help lookup and display remain centralized in HelpManager and existing F1
actions. Settings now routes by context rather than translated tab names.

## Findings and boundaries

The previous global filter suppressed every tooltip, including diagnostic and
overflow data; registration fixes that overreach. Several dialogs previously
fell back to the underlying main tab for F1; explicit contexts correct this.
No business-logic defect was found or changed. The conceptual Complete Set
example in the request was not copied: the implemented intact-Set behavior
and existing Help are the authority.

Disabled actions continue to use existing adjacent status and confirmation
flows. Help is supplemental; this pass does not guarantee native tooltips on
every disabled widget or inside operating-system file pickers. Broader Help,
README, screenshots, missing dedicated topics and terminology polish belong
to M40. No Help hierarchy or screenshot changes are included.

M31 (Multi-Source / User-Extendable Part Catalog) is deferred beyond v0.4.0.
M33 (Rebrickable Custom List to ALT/MOC Requirements) is deferred because the
required Custom List/WishList API is undocumented; the existing exported-CSV
to MOC requirements workflow is retained. Neither is unfinished v0.4.0 scope.
M37 remains frozen; Schema 35 and Protocol 1.5 are unchanged.

## Validation and runtime acceptance

PrintingHelpUx covers default/persisted preference, existing/new controls,
diagnostic preservation, re-enabling, keyboard input, intentionally bare
controls, action status tips, representative source registration contracts,
inherited F1 topics, accessibility registrations and existing Help resources
and theme contrast. Existing UI tests exercise the actual dialogs separately.
Source-contract assertions avoid exact prose matching.

Validated on Windows with Qt 6.10.3 / MinGW 13.1.0: Release configure and
application build passed; all 115 configured test targets explicitly built;
seven focused Help/UI tests passed; final full CTest passed 123/123 in 161.12
seconds. The Debug application build passed (no Debug test build). Both diff
checks passed. Schema 35 and Protocol 1.5 were verified from their constants.
Changes remain uncommitted. Visual runtime acceptance is pending Ray's review.

Run Release configure/build, all configured test targets, focused Help and UI
tests and full CTest. Build the Debug application because Print Audit's help
registration changed; Debug tests are not needed. Check the diff, schema and
protocol constants. Automated checks do not replace Ray's visual acceptance:

1. Enable Show explanatory tooltips in Settings / Appearance and save.
2. Open Add Inventory; inspect Remember Part, Keep Open and Try BrickLink ID.
3. Inspect Builds' type/source and allocation actions.
4. Inspect Procurement's ITEMID, Remember and XML controls.
5. Inspect Part Reference's Send/customization actions and Part cards.
6. Inspect Settings / APIs; help must contain no credential values.
7. Inspect Server trust, pairing, revoke and maintenance controls.
8. Open a 3D model; inspect Prepare, orientation and export explanations.
9. Open LEGO Fit Calibration; inspect feature, Preferred/Verified and package help.
10. Press F1 in Inventory, Builds, Settings tabs, viewer and calibration.
11. Disable explanatory tooltips and save; reopen/hover existing windows. Data diagnostics and F1 should remain available.
12. Re-enable and confirm explanatory hover returns.
13. Restart and confirm the saved preference persists.
14. Check representative hover and Help text in Dark and Light themes.
