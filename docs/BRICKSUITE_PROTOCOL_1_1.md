# BrickSuite Host Protocol 1.1

This document describes the BrickSuite Host protocol implemented through M26.5. It is a
developer reference for maintaining the current Host/read-only-Client architecture. Source code
remains authoritative if this document and the implementation ever disagree.

## 1. Scope and architecture

BrickSuite uses a JSON protocol over TLS-protected WebSockets (`wss://`). The Host owns operational
workshop data: Workspaces, Storage, Inventory and History, Builds and their fulfillment projections,
Collection, and user Part Reference customizations. A Remote Client reads those domains from the
Host. Catalogs, compositions, images, cached provider data, and the built-in Part Reference remain
local to each Client.

Protocol 1.1 has correlated request/response/error messages plus Host-pushed event messages. The
only event currently defined is `shared.invalidated`, which tells Clients that authoritative Host
state may have changed and should be reread. Events contain no operational rows.

Remote Client operational writes are not implemented in protocol 1.1/M26.5. M26.6 is expected to
reuse the same post-commit invalidation model after adding authenticated Host-authoritative writes.

## 2. Transport, trust, and authentication

- Transport is text JSON over WSS. Binary messages are rejected.
- The Host persists a self-signed TLS identity. Its SHA-256 certificate fingerprint identifies the
  Host to the Client.
- A Client explicitly trusts/pins that fingerprint. Only the limited certificate errors expected
  from the pinned self-signed certificate are ignored, and only after the exact DER fingerprint
  matches. Other TLS errors fail closed.
- The Host access token is a 32-byte cryptographically random, base64url-encoded secret stored by
  the platform credential service. It is not sent over the wire.
- `system.hello` returns a 24-byte connection session ID and a fresh 32-byte challenge. The Client
  creates a fresh 32-byte nonce and sends an HMAC-SHA-256 proof in `system.authenticate`.
- The authenticated input binds the session ID, Host challenge, Client nonce, and negotiated
  protocol version under the `BrickSuite-HMAC-v1` context string. Proof comparison is constant-time.
- A challenge expires after 30 seconds and is consumed on the first authentication attempt.
  Authentication failures are increasingly delayed; three failures close the connection.
- Successful authentication currently returns role `FullBrickSuiteClient`. This is a trusted,
  authenticated BrickSuite application role, but protocol 1.1 still exposes read operations only.
- The wire session ID is connection-local authentication material, not a durable Client identity.
  `RemoteSessionState` session and Workspace generations are Client-process lifecycle guards and are
  not durable wire session IDs.
- The Host accepts at most eight simultaneous connections.

## 3. Version and capability negotiation

Every envelope contains `protocol.major` and `protocol.minor`. Major version 1 must match exactly.
The negotiated minor is the lower of the Client and Host minor versions.

After authentication, the Client requests `system.capabilities`. The response includes exact
operation names and semantic capability names. Protocol 1.1 advertises `shared.invalidations`.
A negotiated 1.0 connection, or a Client that did not negotiate that capability, does not receive
invalidation events and continues to use request/response reads and reconnect refresh behavior.

Capabilities currently include:

- `workspace.read`
- `storage.read`
- `inventory.read`, `inventory.detail.read`, `inventory.history.read`
- `builds.read`, `builds.detail.read`, `builds.requirements.read`,
  `builds.missingParts.read`, `builds.pulling.read`
- `collection.read`
- `partReference.customizations.read`
- `shared.invalidations`

## 4. Message envelopes

Envelopes reject unknown top-level fields. `protocol`, `type`, `operation`, and object-valued
`payload` are common fields. Operations and request IDs are limited to 128 characters. Operations
must match the implementation's restricted operation-name syntax.

### Request

```json
{"protocol":{"major":1,"minor":1},"type":"request","requestId":"0e61...","operation":"storage.list","payload":{"workspaceId":3}}
```

`requestId` is required and non-empty for every non-event message.

### Successful response

```json
{"protocol":{"major":1,"minor":1},"type":"response","requestId":"0e61...","operation":"storage.list","payload":{"rows":[]},"success":true}
```

The request ID and operation correspond to the request.

### Error response

```json
{"protocol":{"major":1,"minor":1},"type":"error","requestId":"0e61...","operation":"storage.list","payload":{},"success":false,"error":{"code":"NOT_FOUND","message":"The requested Workspace was not found.","retryable":false}}
```

The `error` object contains a non-empty `code`, user/developer-safe `message`, and Boolean
`retryable` field.

### Event

```json
{"protocol":{"major":1,"minor":1},"type":"event","operation":"shared.invalidated","payload":{"sequence":42,"domains":["inventory","inventoryHistory"],"workspaceId":3,"inventoryRecordId":91}}
```

Events deliberately have no `requestId` or `success` field. They never enter pending-request
correlation. The Host rejects and closes a connection that sends a Client-originated event.

## 5. Request correlation and timeouts

The Client generates UUID request IDs and keeps at most 16 outstanding requests. A request has a
maximum 30-second timeout; callers may request a shorter timeout. Timeout, disconnect, and shutdown
remove pending entries and notify guarded callers. Responses with unknown or stale request IDs are
ignored. Completion callbacks are guarded by their QObject context.

On the Host, an asynchronous response is sent only if the socket still exists, remains authenticated,
and retains the same connection session ID that submitted the request.

## 6. Operations

All listed operations require authentication except that `system.hello` is handled as the initial
handshake. Payloads are strictly validated before a Host read is queued.

| Operation | Request payload | Response purpose |
|---|---|---|
| `system.hello` | Empty | Negotiated version, HMAC method, session ID, challenge, expiry |
| `system.authenticate` | `sessionId`, `clientNonce`, `proof` | Authentication and `FullBrickSuiteClient` role |
| `system.capabilities` | Empty | Versions, schema, capabilities, and operations |
| `system.ping` | Empty | Host UTC timestamp |
| `system.catalogStatus` | Empty | Currently reports unsupported |
| `workspace.list` | Empty | Active Host Workspaces |
| `storage.list` | Positive `workspaceId`; optional Boolean `includeInactive` | Workspace Storage hierarchy |
| `inventory.search` | `workspaceId`, `text`, `storageId`, Rebrickable category/color IDs, `page`, `pageSize` | Paged Inventory |
| `inventory.get` | `workspaceId`, `inventoryRecordId` | One Inventory record |
| `inventory.history` | `workspaceId`, canonical `partNumber`, `rebrickableColorId` | Matching Inventory movement history |
| `builds.list` | `workspaceId`, `includeArchived` | Build summaries |
| `builds.get` | `workspaceId`, `buildId` | One Build |
| `builds.requirements` | `workspaceId`, `buildId`, `page`, `pageSize` | Immutable requirement projection |
| `builds.missingParts` | `workspaceId`, `buildId`, `page`, `pageSize` | Host-calculated Missing Parts |
| `builds.pulling` | `workspaceId`, `buildId`, `page`, `pageSize` | Current read-only Pulling projection |
| `collection.search` | Workspace, text/type/state/condition/completeness/location/active filters and paging | Paged Collection |
| `collection.get` | `workspaceId`, `collectionItemId` | One Collection item |
| `partReference.customizations` | Empty | Host user-customization overlay only |

Pages are one-based and limited to 500 rows. Search text is limited to 512 characters. Storage lists
are limited to 10,000 rows. DTO codecs validate types, ranges, row counts, and required identities;
they do not expose SQL or database connection details.

## 7. `shared.invalidated`

`shared.invalidated` travels only from Host to authenticated protocol-1.1 Clients that completed
capability negotiation. The payload fields are:

- `sequence`: required positive integer assigned by the running Host server;
- `domains`: one to ten unique supported domain strings;
- `workspaceId`: required for Workspace-owned domains;
- optional `buildId`, `inventoryRecordId`, `storageLocationId`, `collectionItemId`;
- optional `partNumber`, limited to 128 characters.

Positive numeric identifiers and sequence values cannot exceed 9,007,199,254,740,991, the largest
integer represented exactly by JSON/IEEE-754 binary64. Unknown fields, duplicate/unknown domains,
invalid scope, or invalid identifiers reject the event. Host-wide domains (`workspaces` and
`partReferenceCustomizations`) cannot be combined with Workspace-scoped domains in one event.

Sequence numbers are process-local diagnostics and ordering aids. They are not durable replay
authority. The Host may restart the sequence, events are not persisted, and Clients do not request
replay.

Invalidations mean data *may have changed*. A Client rereads authoritative state; it must not derive
or apply row mutations from the event itself.

## 8. Invalidation domains and projections

| Wire domain | Intended affected Client projections |
|---|---|
| `workspaces` | Workspace list and selected-Workspace validation |
| `storage` | Storage tree plus Inventory and Collection location-dependent views/filters |
| `inventory` | Inventory, Build availability summaries, Missing Parts, Pulling, and History |
| `inventoryHistory` | Matching open Inventory History, or all open History scope when broad |
| `builds` | Builds list, Missing Parts, and Pulling summaries |
| `buildRequirements` | Selected Build requirements, Missing Parts, and Pulling |
| `missingParts` | Missing Parts projection |
| `pulling` | Existing Pulling dialog, Builds, Missing Parts, Inventory, and History |
| `collection` | Collection list/details |
| `partReferenceCustomizations` | Host customization overlay only; never the built-in manifest or images |

The application-layer `RemoteRefreshCoordinator` coalesces events for 150 ms, tracks dirty
projections, refreshes visible surfaces, defers hidden surfaces until relevant, allows one in-flight
read per projection, and schedules at most one rerun when invalidated in flight. Compound-domain
plans use sets, so overlapping dependencies do not multiply a projection refresh.

## 9. Reconnect and identity semantics

Events are not durable and missed events are not replayed. Disconnect advances the Client session
generation and marks previously loaded Host data stale. Reconnecting to the same verified Host
fingerprint establishes a new session generation, revalidates the remembered Workspace, and forces
a current-state refresh even if the Workspace ID is unchanged.

Changing Workspace advances the Workspace generation and resets coordinator dirty/in-flight state.
Changing to a different trusted Host fingerprint clears prior operational state and the remembered
Workspace context. Async read results carry a snapshot of Host identity, session generation,
Workspace generation, and Workspace ID; results and events from obsolete contexts are rejected.
Client-local catalogs, image caches, and built-in Part Reference data are unaffected.

## 10. Limits and malformed traffic

| Limit | Value |
|---|---:|
| Text message/frame | 1 MiB |
| JSON nesting depth | 16 |
| Request ID | 128 characters |
| Operation | 128 characters |
| Outstanding Client requests | 16 |
| Request timeout | 30 seconds maximum |
| Authentication timeout | 30 seconds |
| Host connections | 8 |
| Authentication failures | 3 before close |
| Read page size | 500 rows |
| Storage locations | 10,000 rows |
| Search text | 512 characters |
| Invalidation domains | 10 |
| Invalidation Part Number | 128 characters |
| Invalidation integer | 9,007,199,254,740,991 |

Malformed JSON, unsupported fields, invalid envelopes, incompatible major versions, invalid payloads,
unauthenticated operations, and excessive messages fail with protocol errors or connection closure as
appropriate. Client-originated events are a policy violation. Server failures use bounded error
payloads and do not expose SQL details.

## 11. Threading and ownership

The WebSocket server owns socket sessions. Host reads are serialized through a dedicated worker with
its own named SQLite connection; `QSqlDatabase` objects never cross threads. Results return through
QObject-guarded queued callbacks.

`OperationalInvalidationPublisher` accepts application-thread publication and queues the broadcast
onto the server thread when required. Its queued lambda uses a `QPointer` guard. Publication occurs
after the business workflow completes and is observational: it is not part of the transaction, and
zero Clients, a stopped server, or a disconnect cannot roll back an authoritative mutation.

The current mutation flow is:

```text
successful Host-local mutation and commit
    -> HostMutationPublicationService
    -> OperationalInvalidationPublisher
    -> eligible authenticated Clients
    -> RemoteSessionState guard
    -> RemoteRefreshCoordinator
    -> targeted authoritative reread
```

## 12. Extending the protocol

When adding an operation or event:

1. Define a stable operation and, where useful, semantic capability name.
2. Decide whether the change is minor-version compatible or requires a new major version.
3. Require authentication and enforce Workspace/entity authorization at the Host boundary.
4. Validate the complete payload strictly, including sizes, enums, paging, and identities.
5. Return DTOs rather than SQLite rows or repository implementation details.
6. Preserve worker-owned database connections and QObject lifetime guards.
7. For mutations, define idempotency/replay and conflict semantics, commit atomically, then invoke
   `HostMutationPublicationService`; never publish before commit.
8. Add envelope, malformed-input, authentication, multi-client, lifecycle, and DTO tests.
9. Update capabilities, compatibility behavior, Help where user-visible, and this document.
