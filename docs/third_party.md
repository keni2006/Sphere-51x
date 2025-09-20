# Third-party components

The repository vendors the header files and documentation of Oracle's
**MySQL Connector/C 6.1.11** under
`third_party/mysql-connector-c-6.1.11-win32`. This keeps the public MySQL C API
headers (`mysql.h`, `mysql_com.h`, etc.) in-tree together with the GPLv2 license
and MySQL FOSS License Exception. Pre-built import libraries and DLLs are not
included; obtain them from your preferred MySQL/MariaDB Connector/C installation
and make them available to the linker/runtime via `MYSQL_LIB_DIR` or the project
settings.

If you prefer a different version of the MySQL or MariaDB C client library you
can override the include/library search paths by setting the
`MYSQL_INCLUDE_DIR` and `MYSQL_LIB_DIR` environment variables before compiling,
or by editing the project configuration directly.
