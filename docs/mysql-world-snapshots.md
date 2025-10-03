# MySQL World Snapshot Exports / Экспорт снимков мира MySQL

<details open>
<summary>English</summary>

## Overview
- SphereServer 0.51x can export a full MySQL world snapshot every time the world saver finishes committing data to the live tables.
- `CWorld::FinalizeStorageSave()` queues the snapshot work through `MySqlStorageService::ScheduleWorldSnapshot()`, allowing the main save to complete while the copy runs on the background worker whenever available.
- If the queue cannot accept the job the server logs a warning and falls back to running the snapshot inline; live tables stay writable because only `SELECT` queries are issued against them.

## When snapshots run
- Snapshots are triggered immediately after the main world-save transaction commits and the save counter increments. The default label is `"World save #<n>"`, matching the new save count.
- Timestamps are rounded down to the configured save period (`SAVEPERIOD`). This ensures recurring saves reuse consistent directory names even if the saver starts a few seconds early or late.
- Administrators can still start manual saves; the helper uses the same schedule and label logic so ad-hoc saves also get a snapshot directory.

## Output location
- Set the `WORLDSAVE` key in `spheredef.ini` to the directory where SphereServer should keep flat-file saves and exported snapshots. The server creates a `mysqlsnapshots/` subdirectory under this path when MySQL snapshots run for the first time.
- Each snapshot is written to `WORLDSAVE/mysqlsnapshots/<timestamp_or_label>/`. The folder name contains the aligned timestamp (`YYYYMMDD_HHMMSS`) followed by an optional sanitized label suffix (for example, `20240314_210000_World_save_12`).
- The Sphere process must be able to create directories and write files under the configured `WORLDSAVE` path. Permission issues are reported in the server log and abort the snapshot.

## Files that are generated
Every snapshot folder contains one SQL dump per world table plus a metadata file:

| File | Contents |
| ---- | -------- |
| `world_objects.sql` | `DROP TABLE`, `CREATE TABLE` (structure) and `INSERT` statements for `<prefix>world_objects`. |
| `world_object_data.sql` | Full dump of `<prefix>world_object_data`. |
| `world_object_components.sql` | Full dump of `<prefix>world_object_components`. |
| `world_object_relations.sql` | Full dump of `<prefix>world_object_relations`. |
| `world_object_audit.sql` | Full dump of `<prefix>world_object_audit`. |
| `gm_pages.sql` | Full dump of `<prefix>gm_pages`. |
| `servers.sql` | Full dump of `<prefix>servers`. |
| `sectors.sql` | Full dump of `<prefix>sectors`. |
| `metadata.txt` | Plain-text key/value pairs describing the label, creation time, object count, save period in seconds and the directory path that was written. |

All SQL files include the table definition retrieved via `SHOW CREATE TABLE` so they can be replayed on a clean schema without additional migrations.

## Database bookkeeping
- The snapshot transaction also inserts a row into `<prefix>world_savepoints` using the generated label and timestamp. This mirrors the previous behaviour that recorded save metadata directly inside MySQL.
- The metadata row is written even though the data lives on disk, letting operators track when the latest snapshot finished and how many objects it contained.

## Failure handling
- Any failure during file creation, data streaming or metadata recording aborts the snapshot transaction and writes an error to the save log channel.
- The world save itself stays committed; the warning clarifies that only the snapshot step failed so administrators can investigate while players keep using the shard.

</details>

<details>
<summary>Русский</summary>

## Обзор
- SphereServer 0.51x умеет выгружать полный снимок мира из MySQL каждый раз, когда основной процесс сохранения завершает фиксацию данных в рабочих таблицах.
- Метод `CWorld::FinalizeStorageSave()` ставит задачу в очередь через `MySqlStorageService::ScheduleWorldSnapshot()`, поэтому главное сохранение завершается сразу, а копирование выполняется в фоновом потоке, если он доступен.
- Если очередь недоступна, сервер пишет предупреждение и выполняет снимок синхронно; рабочие таблицы всё равно остаются доступными для записи, потому что из них читаются только `SELECT`‑запросы.

## Когда создаются снимки
- Снимок запускается сразу после коммита основной транзакции и увеличения счётчика сохранений. Метка по умолчанию — `"World save #<n>"`, где `<n>` соответствует новому значению счётчика.
- Метка времени округляется вниз до настроенного интервала сохранения (`SAVEPERIOD`), поэтому папки снимков имеют стабильные имена даже при небольших отклонениях во времени запуска.
- Администраторы могут запускать ручные сохранения — вспомогательный метод использует те же правила и для них, поэтому разовые сохранения также получают каталог снимка.

## Где лежат файлы
- Параметр `WORLDSAVE` в `spheredef.ini` задаёт каталог, куда сервер пишет классические `sphereworld.scp` и MySQL‑снимки. При первом запуске снимков сервер создаёт в нём подпапку `mysqlsnapshots/`.
- Каждый снимок попадает в `WORLDSAVE/mysqlsnapshots/<timestamp_or_label>/`. Имя папки содержит выровненную метку времени (`ГГГГММДД_ЧЧММСС`) и, при наличии, очищенную версию метки (например, `20240314_210000_World_save_12`).
- Процесс Sphere должен иметь права на создание папок и запись файлов в выбранном каталоге `WORLDSAVE`. Проблемы с доступом отражаются в журнале и отменяют снимок.

## Какие файлы создаются
Внутри каждой папки снимка находятся дампы таблиц и файл с метаданными:

| Файл | Содержимое |
| ---- | ---------- |
| `world_objects.sql` | Команды `DROP TABLE`, `CREATE TABLE` и `INSERT` для `<prefix>world_objects`. |
| `world_object_data.sql` | Полный дамп `<prefix>world_object_data`. |
| `world_object_components.sql` | Полный дамп `<prefix>world_object_components`. |
| `world_object_relations.sql` | Полный дамп `<prefix>world_object_relations`. |
| `world_object_audit.sql` | Полный дамп `<prefix>world_object_audit`. |
| `gm_pages.sql` | Полный дамп `<prefix>gm_pages`. |
| `servers.sql` | Полный дамп `<prefix>servers`. |
| `sectors.sql` | Полный дамп `<prefix>sectors`. |
| `metadata.txt` | Текстовый файл с парами ключ/значение: метка, время создания, количество объектов, период сохранения в секундах и путь к каталогу снимка. |

Каждый SQL‑файл начинается с определения таблицы (`SHOW CREATE TABLE`), поэтому дамп можно восстановить на чистой базе без дополнительных миграций.

## Учёт в базе данных
- Транзакция снимка добавляет запись в `<prefix>world_savepoints`, используя сгенерированную метку и метку времени. Это сохраняет прежнюю возможность отслеживать сохранения прямо в MySQL.
- Строка с метаданными создаётся, даже если данные лежат на диске, что позволяет операторам видеть дату и объём последнего снимка.

## Обработка ошибок
- Любая ошибка при создании каталогов, потоковом чтении данных или записи метаданных приводит к откату транзакции снимка и записи сообщения в лог сохранений.
- Основное сохранение мира остаётся зафиксированным; предупреждение лишь сообщает, что не удалось создать резервную копию, чтобы администраторы могли разобраться без остановки шарда.

</details>
