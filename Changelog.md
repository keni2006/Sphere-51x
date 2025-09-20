### 📅 Date: 05.11.2024
- Documented the new MySQL configuration parameters (`MYSQL`, `MYSQLHOST`,
  `MYSQLPORT`, `MYSQLUSER`, `MYSQLPASS`, `MYSQLDB`, `MYSQLPREFIX`) exposed via
  `spheredef.ini`.
- Described the expanded database schema (accounts, world objects, GM pages,
  timers, metadata tables) and published an example dump at
  `docs/mysql-schema.sql`.
- Upgrade steps for shard administrators:
  1. Install MySQL Server 5.7+ (or MariaDB 10.3+) and the matching client
     libraries.
  2. Update `spheredef.ini` with the new keys and run the server once to execute
     the migrations.
  3. Execute `SAVE 0` to populate the world tables, then verify
     ``SELECT id, version FROM `<prefix>schema_version`;`` returns version 3 and
     import flag 1.

### 📅 Date: 04.11.2024
- Necro book pre setup
- *TODO* do normal book wih wisp etc.
### 📅 Date: 25.10.2024
- Duplicate names function released 
- *TODO* Create normal path for locating sphereworld.scp, create msg fuction for client when name duplicate.
### 📅 Date: 24.10.2024
- Check for duplicate names added
- *TODO* create normal check via method `IsNameTaken`
### 📅 Date: 23.10.2024
- Fix some memory leaks due scripts using
- Clean Garbage
- Added multi lang support
- *TODO* Setup lang via `lang` command(HELP GUMP) for client
### 📅 Date: 21.10.2024
- PVPPOINTS now store correctly in sphereworld
- Added color output in console

### 📅 Date: 18.10.2024

- Should be tested. Currently not working correctly.  
- *TODO*: Should be saved in `sphereworld` like `PVPPOINTS=VALUE`.  
