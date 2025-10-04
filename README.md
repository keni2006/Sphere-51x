# SphereServer-0.51x

<p align="center">
  <img src="https://avatars.githubusercontent.com/u/7201959?s=200&v=4" alt="SphereServer logo" width="120" />
</p>

<p align="center">
  <a href="https://github.com/keni2006/Sphere-51x/releases"><img src="https://img.shields.io/badge/Download-Release-blue.svg" alt="Download" /></a>
  <a href="https://github.com/keni2006/Sphere-51x/issues"><img src="https://img.shields.io/badge/Report-Issue-red.svg" alt="Issues" /></a>
  <a href="https://github.com/keni2006/Sphere-51x/blob/main/CONTRIBUTING.md"><img src="https://img.shields.io/badge/Contribute-Guidelines-green.svg" alt="Contribution" /></a>
</p>

<p align="center">
  <strong>Languages:</strong>
  <a href="#english">English</a>
  ·
  <a href="#russian">Русский</a>
  ·
  <a href="docs/README.md">Docs hub</a>
</p>

---

## 🇺🇸 English <a id="english"></a>

### Overview
SphereServer-0.51x is an evolution of the recovered 0.51a (a.k.a. 0.52) Ultima Online server sources that were originally preserved by Westy. This repository continues the restoration work with modern tooling, optional MySQL storage and gameplay quality-of-life improvements while keeping compatibility with the classic scripting ecosystem.

### Feature highlights
- **Modern persistence:** Optional MySQL backend for accounts, world saves, GM pages and timers with automatic migrations and import of legacy `sphereaccu.scp` data.
- **Gameplay tooling:** PvP point tracking (`src.pvpPoints`), duplicate name prevention and coloured console output for easier moderation.
- **Script localisation:** Extended `sysmessage` syntax (`src.sysmessage Hello world^Привет мир`) to support bilingual shards out of the box.
- **Configurable storage layer:** Connection pooling, charset normalisation and repository helpers contained in `GraySvr/MySqlStorageService.*`.

### Quick start
1. **Clone the repository**
   ```bash
   git clone https://github.com/keni2006/Sphere-51x.git
   cd Sphere-51x
   ```
2. **Fetch the MariaDB Connector/C binaries** (required for the optional MySQL backend):
   ```cmd
   scripts\fetch_mariadb_connector.bat
   ```
3. **Open `GraySvr.sln` in Visual Studio** (2019 or newer is recommended) and select the desired configuration (`Release`/`Debug`).
4. **Configure MySQL** by editing `GraySvr/spheredef.ini` (also known as `sphere.ini`). Use the keys documented in [`docs/database-schema.md`](docs/database-schema.md) and the Russian translation in [`docs/ru/database-schema.md`](docs/ru/database-schema.md).
5. **Launch the shard** and use in-game commands or scripts to verify that accounts and world data are saved correctly.

> Tip: If you prefer a system-wide connector install, set the `MYSQL_INCLUDE_DIR` and `MYSQL_LIB_DIR` environment variables before building.

### Documentation
The repository includes full English and Russian documentation. Use the [Markdown docs hub](docs/README.md) or follow the links below:

| Topic | English | Russian |
| ----- | ------- | ------- |
| Storage architecture | [`docs/database-schema.md`](docs/database-schema.md) | [`docs/ru/database-schema.md`](docs/ru/database-schema.md) |
| Migration checklist | [`docs/storage-migration.md`](docs/storage-migration.md) | [`docs/ru/storage-migration.md`](docs/ru/storage-migration.md) |
| World snapshots | [`docs/mysql-world-snapshots.md`](docs/mysql-world-snapshots.md) | [`docs/ru/mysql-world-snapshots.md`](docs/ru/mysql-world-snapshots.md) |
| Third-party libraries | [`docs/third_party.md`](docs/third_party.md) | [`docs/ru/third_party.md`](docs/ru/third_party.md) |
| Change log | [`Changelog.md`](Changelog.md) | [`docs/ru/Changelog.ru.md`](docs/ru/Changelog.ru.md) |

### Roadmap
The current global goals for the project:

- [ ] Refine network protocol handling.
- [ ] Harden the script engine against invalid input to prevent crashes.
- [x] Ensure PvM/PvP point accrual honours zone restrictions.
- [x] Ship a MySQL datastore that eliminates the need for `SAVEWORLD`.
- [ ] Polish multi-language tooling (`lang` command integration).

### Contributing & support
We welcome bug reports, feature suggestions and pull requests. Please review the [Contribution Guidelines](https://github.com/keni2006/Sphere-51x/blob/main/CONTRIBUTING.md) before submitting changes. Join the discussion via [GitHub Issues](https://github.com/keni2006/Sphere-51x/issues) and keep an eye on the [Changelog](Changelog.md) for the latest updates.

### License
SphereServer-0.51x is distributed under the [MIT License](https://github.com/keni2006/Sphere-51x/blob/main/LICENSE).

---

## 🇷🇺 Русский <a id="russian"></a>

### Обзор
SphereServer-0.51x — это развитие восстановленного исходного кода Ultima Online версии 0.51a (также известной как 0.52), который был сохранён Westy. Репозиторий продолжает работу по реставрации: добавляет современную инфраструктуру, опциональное хранение в MySQL и улучшения игрового процесса, сохраняя при этом совместимость с классической системой скриптов.

### Основные возможности
- **Современное хранение данных.** Опциональный MySQL-бэкенд для аккаунтов, сохранений мира, страниц GM и таймеров с автоматическими миграциями и импортом старого файла `sphereaccu.scp`.
- **Инструменты для геймплея.** Учёт PvP-очков (`src.pvpPoints`), предотвращение дублирования имён и цветной вывод в консоль для удобства модерации.
- **Локализация скриптов.** Расширенный синтаксис `sysmessage` (`src.sysmessage Hello world^Привет мир`) позволяет запускать двуязычные шард‑серверы без дополнительных модификаций.
- **Гибкий слой хранения.** Пул подключений, нормализация кодировок и репозитории, реализованные в файлах `GraySvr/MySqlStorageService.*`.

### Быстрый старт
1. **Клонируйте репозиторий.**
   ```bash
   git clone https://github.com/keni2006/Sphere-51x.git
   cd Sphere-51x
   ```
2. **Загрузите MariaDB Connector/C** (необходим для MySQL-бэкенда):
   ```cmd
   scripts\fetch_mariadb_connector.bat
   ```
3. **Откройте `GraySvr.sln` в Visual Studio** (желательно 2019 и новее) и выберите нужную конфигурацию (`Release` или `Debug`).
4. **Настройте MySQL** через файл `GraySvr/spheredef.ini` (он же `sphere.ini`). Подробности описаны в [`docs/database-schema.md`](docs/database-schema.md) и в русской версии [`docs/ru/database-schema.md`](docs/ru/database-schema.md).
5. **Запустите сервер** и убедитесь, что учётные записи и данные мира корректно сохраняются.

> Совет: Если используется системная установка коннектора, укажите переменные окружения `MYSQL_INCLUDE_DIR` и `MYSQL_LIB_DIR` перед сборкой.

### Документация
Полный комплект документации доступен на английском и русском языках. Можно воспользоваться [Markdown-хабом документации](docs/README.md) или перейти по ссылкам ниже:

| Раздел | Английский | Русский |
| ------ | ---------- | ------- |
| Архитектура хранения | [`docs/database-schema.md`](docs/database-schema.md) | [`docs/ru/database-schema.md`](docs/ru/database-schema.md) |
| Чек-лист миграции | [`docs/storage-migration.md`](docs/storage-migration.md) | [`docs/ru/storage-migration.md`](docs/ru/storage-migration.md) |
| Снимки мира | [`docs/mysql-world-snapshots.md`](docs/mysql-world-snapshots.md) | [`docs/ru/mysql-world-snapshots.md`](docs/ru/mysql-world-snapshots.md) |
| Сторонние библиотеки | [`docs/third_party.md`](docs/third_party.md) | [`docs/ru/third_party.md`](docs/ru/third_party.md) |
| Журнал изменений | [`Changelog.md`](Changelog.md) | [`docs/ru/Changelog.ru.md`](docs/ru/Changelog.ru.md) |

### План работ
Текущие глобальные задачи проекта:

- [ ] Уточнить обработку сетевого протокола.
- [ ] Повысить устойчивость движка скриптов к некорректным данным.
- [x] Убедиться, что PvM/PvP-очки начисляются с учётом зон.
- [x] Реализовать MySQL-хранилище, избавляющее от необходимости `SAVEWORLD`.
- [ ] Завершить интеграцию языковой команды `lang`.

### Участие и поддержка
Мы приветствуем отчёты об ошибках, предложения и pull request'ы. Перед отправкой изменений ознакомьтесь с [руководством по участию](https://github.com/keni2006/Sphere-51x/blob/main/CONTRIBUTING.md). Общение ведётся через [раздел Issues](https://github.com/keni2006/Sphere-51x/issues), актуальные новости публикуются в [журнале изменений](Changelog.md).

### Лицензия
SphereServer-0.51x распространяется по лицензии [MIT](https://github.com/keni2006/Sphere-51x/blob/main/LICENSE).
