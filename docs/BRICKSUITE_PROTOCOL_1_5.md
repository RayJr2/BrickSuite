# BrickSuite Protocol 1.5 Additions

Protocol 1.5 preserves the 1.x negotiated-minor contract and adds the portable
Host-authoritative buildability foundation. Peers advertise and accept the additions
below only when the negotiated minor is at least 5.

## Capabilities and operations

- `buildability.inventory` enables the read-only
  `buildability.inventory.search` and `buildability.inventory.details` operations.
- `collection.partsSource.set` enables the narrow, idempotent Collection Set
  parts-source mutation of the same name.

Buildability requests identify Sets by Set number, Themes by active Rebrickable Theme
ID, Parts by Part number, and Colors by Rebrickable Color ID. Local database row IDs
are not wire identities. Search responses are bounded to 250 compact candidates;
details use pages of at most 500 requirements and at most 500 contributing Collection
sources. The centralized 1 MiB Host outbound limit remains authoritative.

Protocol 1.5 Collection read representations add `allowPartsSource`. Protocol 1.4
representations remain unchanged. The new Host-wide `buildability` invalidation is
removed for peers negotiated below 1.5. Inventory, Builds, Build Requirements,
Pulling, Collection, and Workspace invalidations also make a client buildability
projection stale; UI refresh coordination is introduced in M28.5C2.
