# MySQL Schema Overview

This project relies on a small number of MySQL tables that back account
management and optional world state persistence. The following summary reflects
schema version 3 as created by the migrations in
`GraySvr/CWorldStorageMySQL.cpp`.

## `schema_version`
Stores the current schema revision as well as the flag that indicates whether
a legacy import has been completed.

| Column  | Type        | Notes                                 |
| ------- | ----------- | ------------------------------------- |
| id      | INT         | Primary key. 1 = schema, 2 = import.  |
| version | INT         | Current version/flag value.           |

## `accounts`
Primary record for player accounts.

| Column              | Type                       | Notes |
| ------------------- | -------------------------- | ----- |
| id                  | INT UNSIGNED AUTO_INCREMENT| Primary key. |
| name                | VARCHAR(32)                | Unique login name. |
| password            | VARCHAR(64)                | Stored credential (hashed or plaintext based on configuration). |
| plevel              | INT                        | Privilege level. |
| priv_flags          | INT UNSIGNED               | Bit field with auxiliary privileges. |
| status              | INT UNSIGNED               | Bit field used for jailed/blocked states. |
| comment             | TEXT                       | GM comment. |
| email               | VARCHAR(128)               | Contact e-mail. |
| chat_name           | VARCHAR(64)                | Optional chat handle. |
| language            | CHAR(3)                    | ISO language code. |
| total_connect_time  | INT                        | Accumulated connection time. |
| last_connect_time   | INT                        | Duration of the last session. |
| last_ip             | VARCHAR(45)                | Last IP address (IPv4/IPv6 textual representation). |
| last_login          | DATETIME                   | Timestamp of the last login. |
| first_ip            | VARCHAR(45)                | First IP used by the account. |
| first_login         | DATETIME                   | Timestamp of the first login. |
| last_char_uid       | BIGINT UNSIGNED            | UID of the last played character. |
| email_failures      | INT UNSIGNED               | Failed e-mail delivery attempts. |
| created_at          | DATETIME                   | Creation timestamp. |
| updated_at          | DATETIME                   | Updated automatically via ON UPDATE. |

*Indexes*: `PRIMARY KEY(id)` and `UNIQUE KEY ux_accounts_name(name)`.

## `account_emails`
Stores the scheduled mail queue for an account.

| Column     | Type                | Notes |
| ---------- | ------------------- | ----- |
| account_id | INT UNSIGNED        | FK to `accounts.id`. |
| sequence   | SMALLINT UNSIGNED   | Ordering for queued messages. |
| message_id | SMALLINT UNSIGNED   | Message identifier. |

*Indexes*: `PRIMARY KEY(account_id, sequence)` with a foreign key cascading on
account deletion.

## `characters` (legacy import staging)
A simplified representation used by the import flow.

| Column        | Type                | Notes |
| ------------- | ------------------- | ----- |
| uid           | BIGINT UNSIGNED     | Primary key. |
| account_id    | INT UNSIGNED        | FK to `accounts.id`. |
| name          | VARCHAR(64)         | Character name. |
| body_id       | INT                 | Body tile identifier. |
| position_x/y/z| INT                 | World coordinates. |
| map_plane     | INT                 | Map plane. |
| created_at    | DATETIME            | Creation time. |

## `items` (legacy import staging)
Container for legacy item serialization during migration.

| Column         | Type                | Notes |
| -------------- | ------------------- | ----- |
| uid            | BIGINT UNSIGNED     | Primary key. |
| container_uid  | BIGINT UNSIGNED     | Self-referencing container FK. |
| owner_uid      | BIGINT UNSIGNED     | FK to `characters.uid`. |
| type           | INT UNSIGNED        | Item type identifier. |
| amount         | INT UNSIGNED        | Stack amount. |
| color          | INT UNSIGNED        | Hue value. |
| position_x/y/z | INT                 | Position within container/top-level. |
| map_plane      | INT                 | Map plane. |
| created_at     | DATETIME            | Creation time. |

The table includes indexes on `container_uid` and `owner_uid` with cascading
foreign keys for cleanup.

## `item_props` (legacy import staging)
Key-value store for serialized item properties.

| Column  | Type            | Notes |
| ------- | --------------- | ----- |
| item_uid| BIGINT UNSIGNED | FK to `items.uid`. |
| prop    | VARCHAR(64)     | Property name. |
| value   | TEXT            | Property value. |

Primary key: `(item_uid, prop)` with cascading delete on the item.

## `sectors`, `gm_pages`, `servers`, `timers`
Supporting tables for optional features mirrored from the legacy schema. The
important column types are kept unsigned for identifiers/counters and respect
the foreign keys laid out in the migrations.

## World persistence tables (`schema` version ≥ 3)
The following tables power MySQL backed world persistence.

### `world_objects`
Metadata for each object that appears in the world save.

| Column         | Type                | Notes |
| -------------- | ------------------- | ----- |
| uid            | BIGINT UNSIGNED     | Primary key matching the Sphere UID. |
| object_type    | VARCHAR(32)         | Either `char` or `item`. |
| object_subtype | VARCHAR(64)         | Hex-encoded base ID. |
| name           | VARCHAR(128)        | Optional display name. |
| account_id     | INT UNSIGNED        | Owner account (characters only). |
| container_uid  | BIGINT UNSIGNED     | Parent container when applicable. |
| top_level_uid  | BIGINT UNSIGNED     | Top-level owner for containment resolution. |
| position_x/y/z | INT                 | Cached location or container offset. |
| created_at     | DATETIME            | Creation timestamp. |
| updated_at     | DATETIME            | Auto-updated timestamp. |

*Indexes*: keys on `object_type`, `account_id`, `container_uid`, and
`top_level_uid` assist lookups for synchronization flows. Foreign keys keep the
tree consistent and default to `SET NULL` for soft-orphan handling.

### `world_object_data`
Stores the serialized script blob of the object.

| Column     | Type            | Notes |
| ---------- | --------------- | ----- |
| object_uid | BIGINT UNSIGNED | FK to `world_objects.uid`. |
| data       | LONGTEXT        | Serialized object state. |
| checksum   | VARCHAR(64)     | Hex digest of the serialized payload. |

Primary key: `object_uid` with cascading delete.

### `world_object_components`
Breaks down TAG/VAR style dynamic properties.

| Column      | Type            | Notes |
| ----------- | --------------- | ----- |
| object_uid  | BIGINT UNSIGNED | FK to `world_objects.uid`. |
| component   | VARCHAR(32)     | Component category (e.g., `TAG`). |
| name        | VARCHAR(128)    | Property name. |
| sequence    | INT             | Stable ordering index. |
| value       | LONGTEXT        | Optional value. |

Primary key: `(object_uid, component, name, sequence)` with indexes on
`component` to speed component fetches.

### `world_object_relations`
Represents parent/child relationships such as equipment or container contents.

| Column     | Type            | Notes |
| ---------- | --------------- | ----- |
| parent_uid | BIGINT UNSIGNED | FK to `world_objects.uid`. |
| child_uid  | BIGINT UNSIGNED | FK to `world_objects.uid`. |
| relation   | VARCHAR(32)     | Relation name (`equipped`, `container`). |
| sequence   | INT             | Ordering for siblings. |

Composite primary key `(parent_uid, child_uid, relation, sequence)` with a
secondary index on `child_uid` and cascading deletes on both foreign keys.

### `world_savepoints`
Book-keeping for manual world snapshots.

| Column        | Type                | Notes |
| ------------- | ------------------- | ----- |
| id            | BIGINT UNSIGNED AUTO_INCREMENT | Primary key. |
| label         | VARCHAR(64)         | Optional label. |
| created_at    | DATETIME            | Creation timestamp. |
| objects_count | INT                 | Object count recorded in the snapshot. |
| checksum      | VARCHAR(64)         | Integrity checksum. |

### `world_object_audit`
Audit log of object mutations.

| Column     | Type                | Notes |
| ---------- | ------------------- | ----- |
| id         | BIGINT UNSIGNED AUTO_INCREMENT | Primary key. |
| object_uid | BIGINT UNSIGNED     | FK to `world_objects.uid`. |
| changed_at | DATETIME            | Timestamp of the change. |
| change_type| VARCHAR(32)         | Type/category of modification. |
| data_before| LONGTEXT            | Optional previous serialized state. |
| data_after | LONGTEXT            | Optional new serialized state. |

Indexes exist on `object_uid` to accelerate history lookups.

