/*
 * Apple adds a handful of SPI flags to its sqlite3. Only two of them are used
 * by SecDb, and both are advisory: data protection classes do not exist on
 * PureDarwin, and incremental auto-vacuum is sqlite's own documented mode.
 */
#ifndef PD_SQLITE3_PRIVATE_H
#define PD_SQLITE3_PRIVATE_H

#include <sqlite3.h>

#ifndef SQLITE_OPEN_FILEPROTECTION_NONE
#define SQLITE_OPEN_FILEPROTECTION_NONE 0x00400000
#endif

#ifndef SQLITE_AUTO_VACUUM_INCREMENTAL
#define SQLITE_AUTO_VACUUM_INCREMENTAL 2
#endif

#endif /* PD_SQLITE3_PRIVATE_H */
