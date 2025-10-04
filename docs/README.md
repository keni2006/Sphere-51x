# SphereServer-0.51x Documentation

> Toggle the sections below to browse the restored knowledge base in English or Russian. This Markdown version renders directly on GitHub, so all links stay clickable without enabling scripts.

## Quick links

| Topic | English | Русский |
| ----- | ------- | ------- |
| Storage architecture | [database-schema.md](database-schema.md) | [ru/database-schema.md](ru/database-schema.md) |
| Migration checklist | [storage-migration.md](storage-migration.md) | [ru/storage-migration.md](ru/storage-migration.md) |
| World snapshots | [mysql-world-snapshots.md](mysql-world-snapshots.md) | [ru/mysql-world-snapshots.md](ru/mysql-world-snapshots.md) |
| Third-party libraries | [third_party.md](third_party.md) | [ru/third_party.md](ru/third_party.md) |
| Change log | [../Changelog.md](../Changelog.md) | [ru/Changelog.ru.md](ru/Changelog.ru.md) |

<details open>
<summary><strong>🇺🇸 English overview</strong></summary>

### Featured guides
- **Storage architecture** — understand how the MySQL facade, connection manager and repository layers cooperate to persist accounts, world objects and timers.
- **Migration checklist** — follow a step-by-step plan for upgrading an existing shard to the refactored MySQL storage stack.
- **World snapshot exports** — review how automated MySQL world snapshots are scheduled, stored on disk and tracked through the schema.
- **Third-party components** — see which MariaDB/MySQL client packages are bundled and how to keep them up to date across build environments.

### Getting started
1. Clone the repository: `git clone https://github.com/keni2006/Sphere-51x.git`
2. Fetch MariaDB Connector/C with `scripts\fetch_mariadb_connector.bat`.
3. Open `GraySvr.sln` in Visual Studio (2019 or newer).
4. Configure the MySQL backend in `GraySvr/spheredef.ini` using the keys described in the documents above.

</details>

<details>
<summary><strong>🇷🇺 Русский обзор</strong></summary>

### Избранные руководства
- **Архитектура хранения** — описывает взаимодействие фасада MySQL, менеджера подключений и слоя репозиториев при сохранении аккаунтов, объектов мира и таймеров.
- **Чек-лист миграции** — пошаговый план перехода существующего шарда на обновлённый стек хранения MySQL.
- **Экспорт снимков мира** — показывает, как планируются и сохраняются автоматические MySQL-снимки мира.
- **Сторонние компоненты** — перечисляет пакеты MariaDB/MySQL и даёт советы по их обновлению в среде сборки.

### Быстрый старт
1. Клонируйте репозиторий: `git clone https://github.com/keni2006/Sphere-51x.git`
2. Запустите `scripts\fetch_mariadb_connector.bat`, чтобы скачать MariaDB Connector/C.
3. Откройте `GraySvr.sln` в Visual Studio 2019+.
4. Настройте MySQL в `GraySvr/spheredef.ini`, опираясь на документы из таблицы.

</details>
