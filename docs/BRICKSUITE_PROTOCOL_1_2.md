# BrickSuite Protocol 1.2

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

M26.6B registers one production mutation operation: `builds.pulling.record`, advertised
as capability `builds.pulling.write`. Inventory, Storage, general Builds, Collection,
and Part Reference remain read-only.

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
 -> post-commit HostMutationPublicationService notification
 -> authoritative response
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
