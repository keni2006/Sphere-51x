# Third-party components

> 🇷🇺 For Russian, see [`docs/ru/third_party.md`](docs/ru/third_party.md).


SphereServer relies on the MariaDB/MySQL client libraries for its optional
database backend. The repository keeps the legacy Oracle headers checked in for
compatibility and ships a helper script that downloads the current MariaDB
packages on demand:

* **MariaDB Connector/C 3.3.8** (LGPL 2.1) can be fetched with
  `scripts\fetch_mariadb_connector.bat`. The script downloads the official
  Windows MSI and Ubuntu tarball, extracts them into `third_party/` and keeps the
  directories out of Git via `.gitignore`. Run it whenever you need to refresh
  the vendor drop. PowerShell 5+, `msiexec` and the `tar` utility available on
  modern Windows installations are required. If you prefer to keep a copy of the
  headers and libraries under version control, drop them into
  `MariaDBConnector/`; the Visual Studio project now looks there in addition to
  `third_party/`.
* **MySQL Connector/C 6.1.11** (GPLv2 + FOSS License Exception) remains checked
  in under `third_party/mysql-connector-c-6.1.11-win32` for projects that still
  rely on the older headers/import libraries.

If you prefer a system-wide installation you can override the include and
library search paths using the `MYSQL_INCLUDE_DIR` and `MYSQL_LIB_DIR`
environment variables or by editing the IDE project settings.
