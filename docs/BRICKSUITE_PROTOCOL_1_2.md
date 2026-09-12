# BrickSuite Protocol 1.2

## Manufacturer choices

The authenticated `manufacturers.list` read takes no payload and returns the active Manufacturer
display names owned by the Host. Remote Build forms use these portable names for choices and send
the selected name in the existing mutation contract; Client-local Manufacturer row IDs never cross
the wire. The current value from an authoritative Build is retained in Edit even when it is no
longer present in the active list.

## Storage reads and mutations

Protocol 1.2 adds authenticated `storage.get` and `storage.types.list` reads with matching exact
capabilities. `storage.get` accepts positive `workspaceId` and `storageId` values and returns the
Host-authoritative location identity, nullable parent, type identity/name, name, description, sort
order, active and capability flags, UTC creation/modification timestamps, and derived display path.
`storage.types.list` takes no payload and returns active Host Storage type IDs, names, and
descriptions. These IDs are Host identities; Client-local type IDs are never substituted.

The independently advertised `storage.add`, `storage.edit`, and `storage.setActive` operations use
the standard mutation envelope and exact same-named capabilities. Add accepts name, optional
description, nullable parent (zero denotes root), Host type ID, and Inventory/Collection capability
flags. Edit adds the target Storage ID and a complete expected prior snapshot. Set-active carries
the target, desired state, and the same expected snapshot. Unknown fields, unsafe IDs, malformed
booleans, and overlong text are rejected before worker execution.

The Host applies all three operations through its connection-bound Storage mutation service on the
serialized write connection. That service validates Workspace ownership/activity, parent and type
state, hierarchy cycles, active children, Inventory and Collection occupancy, capability removal,
and reactivation prerequisites. A changed expected snapshot returns `STALE_VERSION`; validation
and business conflicts are definitive and create no receipt. Successful results include `created`
and the full authoritative Storage detail, so a Client need not perform an immediate follow-up read.

The receipt and Storage change commit atomically. Same-ID/same-payload retry returns the stored
result with `replayed=true`; changed payload returns `IDEMPOTENCY_CONFLICT`. Only timeout or
disconnect creates an unknown outcome. After a new commit, the correlated response is handed to
the server send path before one Workspace/Storage-scoped `Storage` invalidation and Host-local
Storage notification are published. Replays and failures publish nothing. Protocol 1.1 and Hosts
without an exact operation capability remain read-only.

## Inventory mutations

Protocol 1.2 Hosts advertise seven independently authorized Inventory operations and matching
capabilities: `inventory.add`, `inventory.edit`, `inventory.move`, `inventory.correct`,
`inventory.remove`, `inventory.markLost`, and `inventory.markFound`. Protocol 1.1 sessions do not
receive these operations or capabilities.

Every request uses the standard durable mutation envelope (`workspaceId`, `mutationId`, `expected`,
and `mutation`). Part identity is a canonical Part number, Color identity is the Rebrickable Color
ID, Manufacturer identity is its normalized name, and Storage identity is the Host Storage ID from
remote operational reads. Client-local catalog row IDs are never authoritative.

Record mutations include the expected Inventory record ID, quantity, Storage ID, and `modifiedUtc`.
The Host rejects stale state with `STALE_VERSION`; invalid Workspace ownership, catalog identities,
quantities, enumerated values, or non-inventory-capable destinations are rejected before mutation.
Results identify source, destination, and surviving Inventory records where applicable, resulting
quantities and Storage, the modification token, and create/merge outcomes.

Domain changes, movement History, and the durable receipt commit atomically. Repeating the same
mutation ID and payload returns the stored result with `replayed=true`; changing the payload produces
`IDEMPOTENCY_CONFLICT`. Timeouts or disconnects have unknown outcome and must be retried with the
exact same mutation ID and payload. Newly committed operations publish Workspace-scoped Inventory,
Inventory History, Builds, Build Requirements, Missing Parts, and Pulling invalidation; replay and
failed operations publish nothing.

Remote Inventory uses the same user-facing workflows as local Inventory while translating selections
to portable identities before asynchronous submission. The read-only `inventory.lost.list` operation
returns the Host's outstanding Lost Part/Color projection so Mark Found never requires raw identity
entry on the Client.

Protocol 1.2 extends the frozen Protocol 1.1 read and invalidation contract with the
domain-neutral infrastructure required for Host-authoritative mutations. M26.6A does
not expose any operational write operation or enable any Remote Client write control.

## Compatibility

The major version remains 1. A 1.1 Client can authenticate to a 1.2 Host and continues
to receive the 1.1 read operations and `shared.invalidated` events. Operations whose
minimum version is 1.2 and their capabilities are omitted from that session's
capability response. A 1.2 Client connected to a 1.1 Host receives no mutation
capabilities and remains read-only.

Protocol 1.1 envelopes are unchanged. See `BRICKSUITE_PROTOCOL_1_1.md` for transport,
TLS fingerprint pinning, HMAC authentication, correlation, read DTOs, invalidations,
message limits, and reconnect behavior.

## Mutation envelope

Mutations use the normal correlated request/response envelope. The transport
`requestId` is not an idempotency key. Common mutation metadata is inside `payload`:

```json
{
  "protocol": {"major": 1, "minor": 2},
  "type": "request",
  "requestId": "transport-correlation-id",
  "operation": "future.domain.operation",
  "payload": {
    "workspaceId": 7,
    "mutationId": "f7f84dd8-92be-41ef-8e81-bdb27322c494",
    "expected": {},
    "mutation": {}
  }
}
```

`workspaceId` is a positive exact JSON integer. `mutationId` is a UUID generated once
per logical user action. `expected` holds operation-specific conflict inputs and
`mutation` holds its business DTO. Later phases define those operation-specific fields.

## Canonical request hash

The Host hashes the operation, Workspace ID, expected state, and mutation DTO using
SHA-256. JSON object keys are recursively sorted before compact serialization, so key
order does not affect identity. Transport request IDs, socket state, and session
generation are excluded. Arrays remain ordered because order can be business data.

## Durable idempotency

Schema 35 stores successful mutation receipts in `remote_mutation_receipt`. The receipt
contains the mutation ID, operation, Workspace, canonical hash, result code,
authoritative result JSON, commit time, and a bounded diagnostic client identity. It
does not contain credentials, HMAC material, sockets, or the full request payload.

The receipt and domain mutation commit in the same SQLite transaction:

- Unknown ID: execute once, store the authoritative result, and commit.
- Same ID and hash: return the stored result with `replayed=true`; do not execute again.
- Same ID with different operation, Workspace, or hash: return
  `IDEMPOTENCY_CONFLICT`.
- Receipt lookup works after Host restart and through normal database backup/restore.

Receipts older than 90 days are removed in bounded batches of at most 250 when the
Host write worker starts. The committed-time index makes this cleanup bounded. Fresh
receipts remain available across practical disconnect and retry windows.

## Unknown outcomes

A 30-second Client timeout or connection loss after submission does not prove rollback.
It is an unknown outcome. The Client retains the original mutation ID and may offer an
explicit retry using that same ID. M26.6A never resubmits automatically.

If the Host accepted work before disconnect, queued or executing work may continue.
If SQLite commits, the receipt and domain result survive even when the response is
lost. If the Host terminates before commit, SQLite rolls back both. Queued work captures
immutable request values and never owns a socket pointer.

## Serialized write executor

`HostWriteExecutor` owns a dedicated thread and a named read/write QSQLITE connection.
The connection is created, used, closed, and removed on that thread. It enables foreign
keys and a 5000 ms busy timeout, and does not change journal mode or enable WAL.

At most 16 accepted/executing mutations are held. Overflow and bounded SQLite lock
exhaustion return retryable `BUSY`. New work is rejected during shutdown; accepted work
is drained before the connection and thread stop. Host-local GUI writes continue using
the default connection in M26.6A.

## Common result

Successful mutation responses contain:

```json
{
  "mutationId": "f7f84dd8-92be-41ef-8e81-bdb27322c494",
  "operation": "future.domain.operation",
  "replayed": false,
  "committedUtc": "2026-09-10T18:22:31.420Z",
  "authoritative": {}
}
```

The authoritative object is operation-specific and never contains raw SQL rows.

## Errors

The mutation foundation recognizes these stable machine-readable codes:

- `AUTH_REQUIRED`
- `UNKNOWN_OPERATION`
- `INVALID_ARGUMENT`
- `FORBIDDEN`
- `NOT_FOUND`
- `WORKSPACE_NOT_FOUND`
- `WORKSPACE_INACTIVE`
- `CONFLICT`
- `STALE_VERSION`
- `IDEMPOTENCY_CONFLICT`
- `BUSY`
- `TIMEOUT`
- `INTERNAL_ERROR`

Errors include a safe message and retryable flag. SQL errors, paths, secrets, tokens,
stack information, and sensitive request payloads are never returned.

## Capabilities and authorization

Operations can declare an exact capability and minimum protocol minor version. The
Host filters operation and capability advertisement by the negotiated session version.
An authenticated `FullBrickSuiteClient` may invoke only an explicitly registered
operation supported by its negotiated protocol. A broad write marker, if ever added,
must not authorize an operation by itself.

Production Protocol 1.2 mutation capabilities currently cover interactive Pulling, Inventory,
Storage, and the Part Reference customization operations documented below. General Builds and
Collection remain read-only.

## Part Reference customization mutations

Authenticated Hosts advertise `partReference.customizations.add` and
`partReference.customizations.remove` as independent exact capabilities. Built-in Part Reference
content remains Client-local and immutable; only the Host-global user customization overlay is
mutated. The common envelope continues to carry an active `workspaceId` for mutation infrastructure
and receipt identity, but customization ownership is Host-global and the value is neither persisted
on nor used to own a customization.

Add carries canonical `partNumber`, built-in `catalog` and `section` names, `placement` (`Append`,
`Before`, or `After`), and an `anchorPartNumber` for relative placement. Append requires an empty
anchor. The Host resolves the Part against its catalog and independently validates the built-in
manifest destination and effective anchor context; Client Part IDs, row indexes, and display
positions are never authoritative. Existing local duplicate semantics apply: a Part already present
as either a built-in or user entry cannot be added again.

Remove carries the positive Host customization ID and an expected snapshot containing
`modifiedUtc`, Part number, catalog, section, placement, and anchor identity. The Host removes only
a stored user entry. A changed expected snapshot returns `STALE_VERSION`; built-in entries have no
removable Host customization identity.

Both authoritative results return the customization ID and identity snapshot, including creation
and modification UTC timestamps. The customization change and durable receipt commit atomically.
Same-ID/same-payload retry replays the stored result without another mutation or invalidation;
changed payload returns `IDEMPOTENCY_CONFLICT`. Only timeout or disconnect has unknown outcome.
For a new commit, the correlated response precedes one Host-global `PartReferenceCustomizations`
invalidation and Host-local overlay refresh. Replay and failure publish nothing.

## Interactive Pulling mutation

`builds.pulling.record` is available only to an authenticated protocol 1.2
`FullBrickSuiteClient`. Its common metadata carries the Workspace and durable mutation
ID. `mutation` carries a positive Build ID and 1–500 rows. Each row contains an
allocation ID, a positive pull delta, and expected allocated, Inventory, and requirement
pulled quantities. `expected.buildStatus` may carry the state observed by the Client.

The Host validates the complete batch before changing state. The active Build must be a
Stock Build in Planned or Pulling status; every allocation, requirement, Inventory row,
Part/Color, Storage, and Workspace relationship must still agree. Duplicate allocation
IDs, non-positive or overflowing quantities, over-allocation, insufficient Inventory,
and collective requirement over-pulls are rejected. Multiple allocations for one
requirement are compared to the same pre-mutation pulled total before execution.

The authoritative result contains the Build ID, submitted row and piece totals, affected
allocation IDs, resulting requirement pulled totals, and Build status. A stale expected
value returns `CONFLICT` without partial effects. The mutation uses positive deltas; it
never replaces an absolute pulled total.

The Pulling domain updates, movement/provenance records, and receipt share the Host write
transaction. A same-ID/same-payload replay returns the stored result with `replayed=true`
without consuming Inventory, adding history, or publishing again. A changed payload with
the same ID returns `IDEMPOTENCY_CONFLICT`. Timeout or disconnect after submission is an
unknown outcome; the Client retains the exact payload and mutation ID for an explicit
safe replay.

After a new commit, the Host publishes the existing Pulling compound invalidation for the
Workspace and Build: Inventory, Inventory History, Builds, Build Requirements, Missing
Parts, and Pulling. Protocol 1.1 and 1.2 Hosts that do not advertise
`builds.pulling.write` remain read-only.

Each future operation must validate its Workspace and target ownership, entity state,
payload bounds, expected-state conflict inputs, and domain rules on the Host. Client
display data is never authoritative. Administrative backup, restore, schema, TLS,
token, server-setting, and import operations are outside this mutation contract.

## Host execution and publication order

The intended operation path is:

```text
authenticated request
 -> common and operation DTO validation
 -> bounded HostWriteExecutor queue
 -> receipt lookup
 -> Workspace/domain validation
 -> domain mutation + receipt transaction
 -> commit
 -> authoritative response handed to the server send path
 -> post-commit HostMutationPublicationService notification
```

No invalidation is published on validation failure or rollback. Publication failure
does not undo or misreport a committed mutation. Idempotent replay returns the stored
result and does not republish an old invalidation; normal reconnect refresh and current
state reads provide convergence.

## Security limits

Protocol 1.1 frame, nesting, operation, request-ID, authentication, pending-request,
and client-count limits remain in force. Mutation metadata additionally requires a
valid UUID and positive exact Workspace ID. Each later operation must impose tighter
collection counts, string lengths, numeric ranges, duplicate checks, and computational
limits before queue acceptance.

Mutation logs contain only bounded operation, Workspace, abbreviated mutation ID,
outcome, queue/transaction timing, and replay/conflict state. Credentials and full
payloads are excluded.

## Extension guidance

M26.6B and later phases add typed operation DTOs to `RemoteMutationDtos`, a Host-side
connection-bound application/domain service, an exact operation/capability registration,
and a Client method on `RemoteMutationApplicationServices`. Widgets use that application
service and never construct protocol JSON.

The domain mutation and receipt must share one transaction. Business validation must
be repeated inside that transaction. `HostMutationPublicationService` runs only after
commit. Any timeout-capable UI must preserve the mutation ID while the outcome is
unknown.

## Collection mutations

Authenticated Hosts advertise `collection.add`, `collection.edit`, and `collection.setActive`
as independent exact capabilities. Read access does not imply Collection write access.
Add carries exactly one portable source identity: a Rebrickable Set number, a Rebrickable
Minifig number, or a Host Build ID. Client-local catalog row IDs are never authoritative.

Edit and lifecycle requests carry the prior authoritative item as expected state, including
`modifiedUtc`, active state, immutable source identity, Storage ID, and mutable values. The Host
rejects stale state instead of overwriting newer data. Successful replies return the authoritative
item. Standard Protocol 1.2 receipts make an identical retry a replay and reject a changed payload
under the same mutation ID. Only timeout or disconnection has an unknown client outcome.

A newly committed mutation sends its correlated response before publishing one `Collection`
invalidation. Receipt replay publishes no second invalidation, and Collection writes do not emit
synthetic Inventory or Build invalidations.

## Build metadata and lifecycle mutations

Authenticated Protocol 1.2 Hosts advertise `builds.add`, `builds.edit`,
`builds.setActive`, `builds.complete`, and `builds.cancel` as independent exact
capabilities. They do not imply requirement, allocation, disassembly, spare-storage,
or generic status-write access. `builds.pulling.write` remains independent.

`builds.add` accepts Set or MOC portable references, `Stock` or applicable
`CompleteSet` inventory mode, a Host-resolved Manufacturer name when applicable,
name, notes, and only the safe initial `Planned` state. The Host resolves its own
catalog and Manufacturer identities; Client-local primary keys are never authority.
The authoritative result contains the stable Build ID, Workspace, metadata, lifecycle
state, catalog-link indication, and UTC timestamps.

Existing-Build requests carry the complete prior authoritative Build snapshot as
expected state: `modifiedUtc`, active and lifecycle state, immutable type/reference/
inventory mode, Manufacturer display identity, name, and notes. `builds.edit` can
change only name, applicable Manufacturer, and notes. Completion, cancellation, and
archive/reactivation use their semantic operations; there is no `builds.setStatus`.
The Host rejects stale snapshots before mutation.

Cancellation is one atomic operation. When pieces have been pulled, its bounded
return plan identifies the Build requirement, Host-resolved Manufacturer name,
destination Storage ID, quantity, and spare state for every returned provenance row.
The Host revalidates all rows and performs Inventory returns, history/provenance
updates, allocation release, linked Collection synchronization, Build cancellation,
and the durable receipt in one transaction. Closing the Client selection dialog before
submission changes nothing.

Standard receipt behavior applies to every Build operation: same mutation ID and
payload returns the original authoritative result without a second mutation or
invalidation; changed payload returns `IDEMPOTENCY_CONFLICT`. Only timeout or
disconnection is an unknown outcome, for which the Client retains the exact mutation
ID and logical payload for `Retry Safely`.

Add, edit, and active-state changes invalidate Builds. Completion additionally
invalidates Build Requirements, Missing Parts, and Pulling. Cancellation invalidates
those projections and adds Inventory/Inventory History and Collection only when the
committed result changed those domains. The correlated response is handed to the send
path before post-commit invalidation and Host-local refresh.

## Build requirement and allocation mutations

Protocol 1.2 advertises the exact capabilities `builds.requirements.add`,
`builds.requirements.edit`, `builds.requirements.remove`, `builds.allocations.set`,
and `builds.allocateAvailable`. These capabilities are independent of Build metadata,
Pulling, disassembly, and spare-storage access.

Requirement Add identifies original and optional substitute Parts by canonical Part number
and Colors by Rebrickable Color ID; Client-local catalog row IDs are never sent. Edit and
Remove carry a complete expected requirement snapshot including stable requirement/Build IDs,
UTC modification time, original and substitute identities, quantities, and spare state. The
Host resolves identities without creating reference data, verifies Workspace/Build ownership,
and rejects stale or lifecycle-ineligible mutations. A substitution cannot change after an
allocation exists, and quantity cannot be reduced below pulled plus allocated pieces.

`builds.allocations.set` supplies the complete desired allocation set for one requirement,
bounded to 500 rows. Host Inventory record IDs are valid operational identities because they
come from Host reads. Each row carries its observed allocation identity/quantity and Inventory
quantity/version data. The Host revalidates Workspace ownership, effective Part/Color,
availability, duplicates, totals, and expected state before atomically replacing the set.
Allocation reserves Inventory but does not consume it or emit Inventory invalidation.

`builds.allocateAvailable` sends only the Build expected snapshot and optional preferred Host
Storage ID. Candidate selection and allocation occur once on the Host under the existing local
allocator policy. The authoritative result records the resulting allocation rows and totals.
Receipt replay returns that stored result and never recalculates against newer Inventory.

Requirement and allocation commits invalidate Builds, Build Requirements, Missing Parts, and
Pulling. The correlated response precedes invalidation and Host-local refresh. Same mutation ID
and payload replays the stored outcome without another mutation or invalidation; a changed
payload conflicts. Only timeout or disconnect is an unknown outcome and requires retrying the
exact retained request.

## Build disassembly and Complete Set spare storage

Protocol 1.2 advertises `builds.disassemble` and `builds.spare.store` as independent exact
capabilities. Neither implies a generic status or Inventory-write capability. Before disassembly,
`builds.disassemblyReturns` supplies a bounded return projection (maximum 500 rows) containing
Host requirement identity, portable Manufacturer identity, authoritative quantity, and spare
state. The mutation supplies the chosen active Host Inventory leaf Storage IDs and the complete
unchanged return plan; Client row numbers and local catalog or Storage IDs are never authority.

The Host revalidates the complete Build snapshot, ownership, lifecycle, requirement quantities,
manufacturer provenance, Storage destinations, and return-plan completeness inside the write
transaction. Inventory return/merge, movement history, provenance and requirement updates, Build
status transition, and linked Collection synchronization commit atomically. The authoritative
result contains the Build plus affected requirement, Inventory, and allocation IDs and returned
piece count. It invalidates Build projections and only the Inventory/History and Collection
domains actually changed.

`builds.spare.store` identifies one Complete Set spare requirement, a positive bounded quantity,
and an active Host Inventory leaf Storage ID. Its expected requirement snapshot prevents stale
release counts. The Host derives Part, Color, and Manufacturer from the Build requirement and
atomically adds or merges Inventory, records `SetSpareRelease` movement history, and advances the
released quantity.

Both operations use standard durable mutation receipts. Identical mutation ID and payload replay
the original result without duplicate Inventory, history, lifecycle changes, or invalidation;
changed payload conflicts. Only timeout or disconnection is an unknown outcome, and safe retry
must retain the exact mutation ID and frozen plan. The correlated response precedes invalidation
and Host-local refresh.
